
/**
 ******************************************************************************
 * @file         console.c
 * @author       Aaron Escoboza, Github account: https://github.com/aaron-ev
 * @brief        Command Line Interpreter based on FreeRTOS and STM32 HAL layer
 ******************************************************************************
 */
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include "task.h"
#include "queue.h"
// #include "stm32f401xc.h"
#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "hf_common.h"
#include "web-server.h"
#include "hf_power_process.h"
#include "hf_spi_slv.h"

#define CONSOLE_VERSION_MAJOR                   1
#define CONSOLE_VERSION_MINOR                   0

#define MAX_IN_STR_LEN                          300
#define MAX_OUT_STR_LEN                         600
#define MAX_RX_QUEUE_LEN                        300

                                                      /* ASCII code definition */
#define ASCII_TAB                               '\t'  /* Tabulate              */
#define ASCII_CR                                '\r'  /* Carriage return       */
#define ASCII_LF                                '\n'  /* Line feed             */
#define ASCII_BACKSPACE                         '\b'  /* Back space            */
#define ASCII_FORM_FEED                         '\f'  /* Form feed             */
#define ASCII_DEL                               127   /* Delete                */
#define ASCII_CTRL_PLUS_C                         3   /* CTRL + C              */
#define ASCII_NACK                               21   /* Negative acknowledge  */

char cRxData;
QueueHandle_t xQueueRxHandle;
UART_HandleTypeDef *pxUartDevHandle;
static const char *pcWelcomeMsg = "Welcome to the console. Enter 'help' to view a list of available commands.\r\n";

static const char *prvpcTaskListHeader = "Task states: Bl = Blocked, Re = Ready, Ru = Running, De = Deleted,  Su = Suspended\r\n"\
                                         "Task name         State  Priority  Stack remaining  CPU usage  Runtime(us)\r\n"\
                                         "================= =====  ========  ===============  =========  ===========\r\n";
static const char *prvpcPrompt = "#cmd: ";

/* Command function prototypes */
static BaseType_t prvCommandCarrierBoardInfoGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandSomBoardInfoGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

static BaseType_t prvCommandAccountGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandAccountSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

static BaseType_t prvCommandNetInfoGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandIPSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandNetMaskSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandGateWaySet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandMacSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

static BaseType_t prvCommandDipSwitchSoftGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandDipSwitchSoftSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

static BaseType_t prvCommandRtcGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandRtcDateSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandRtcTimeSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

// get the temperature and fan speed
static BaseType_t prvCommandTempGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

// get the overall power consumption, current and voltage
static BaseType_t prvCommandPwrDissipationGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

// get the power status of the som board: on or off
static BaseType_t prvCommandSomPwrStatusGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
// power off or power on the som board
static BaseType_t prvCommandSomPwrStatusSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

// get the software status of the som board: running or stopped
static BaseType_t prvCommandSomSwWorkStatusGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
// reboot the som board
static BaseType_t prvCommandReboot(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

// get the software version of the BMC(Baseboard Management Controller, aka mcu software verrsion)
static BaseType_t prvCommandBMCVersion(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

// read the memory/io address space of som, this is a backdoor for sniffering the som statuer
static BaseType_t prvCommandDevmemRead(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
// write the memory/io addrerss space of som
static BaseType_t prvCommandDevmemWrite(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

static BaseType_t prvCommandEcho( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandTaskStats( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandHeap(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvCommandTicks(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

/**
*   @brief  This function is executed in case of error occurrence.
*   @retval None
*/
static const char *prvpcMapTaskState(eTaskState eState)
{
    switch (eState)
    {
        case     eReady: return "Re";
        case   eRunning: return "Ru";
        case   eDeleted: return "De";
        case   eBlocked: return "Bl";
        case eSuspended: return "S";
        default: return "??";
    }
}

static const CLI_Command_Definition_t xCommands[] =
{
    {
        "stats",
        "\r\nstats: Displays a table with the state of each FreeRTOS task.\r\n",
        prvCommandTaskStats,
        0
    },
    {
        "cbinfo",
        "\r\ncbinfo: Display carrierboard information.\r\n",
        prvCommandCarrierBoardInfoGet,
        0
    },
    {
        "sominfo",
        "\r\nsominfo: Display somboard information.\r\n",
        prvCommandSomBoardInfoGet,
        0
    },
    {
        "account-g",
        "\r\naccount-g: Show account name and password.\r\n",
        prvCommandAccountGet,
        0
    },
    {
        "account-s",
        "\r\naccount-s <name> <password>: Set account name and password.\r\n",
        prvCommandAccountSet,
        2
    },
    {
        "ifconfig",
        "\r\nifconfig: Display network configuration.\r\n",
        prvCommandNetInfoGet,
        0
    },
    {
        "setip",
        "\r\nsetip <ipaddrr>: Set ip address.\r\n",
        prvCommandIPSet,
        1
    },
    {
        "setmask",
        "\r\nsetmask <netmask>: Set netmask address.\r\n",
        prvCommandNetMaskSet,
        1
    },
    {
        "setgateway",
        "\r\nsetgateway <netmask>: Set gateway address.\r\n",
        prvCommandGateWaySet,
        1
    },
    {
        "setmac",
        "\r\nsetmac <mac,like a1:26:39:91:b0:22>: Set mac address.\r\n",
        prvCommandMacSet,
        1
    },
    {
        "bootsel-g",
        "\r\nbootsel-g: Show software bootsel configuration.\r\n",
        prvCommandDipSwitchSoftGet,
        0
    },
    {
        "bootsel-s",
        "\r\nbootsel-s <hw/sw> <bootsel(hex), like, F>: Set software bootsel configuration.\r\n",
        prvCommandDipSwitchSoftSet,
        2
    },
    {
        "date",
        "\r\ndate: Get the current date and time.\r\n",
        prvCommandRtcGet,
        0
    },
    {
        "date-s",
        "\r\ndate-s <Year> <Month> <Date> <WeekDay 1-7>: Set a new date.\r\n",
        prvCommandRtcDateSet,
        4
    },
    {
        "time-s",
        "\r\ntime-s <Hours> <Minutes> <Seconds>: Set a new time.\r\n",
        prvCommandRtcTimeSet,
        3
    },
    {
        "temp",
        "\r\ntemp: Get the temperature and fan speed.\r\n",
        prvCommandTempGet,
        0
    },
    {
        "pd",
        "\r\npd: Get the power dissipation\r\n",
        prvCommandPwrDissipationGet,
        0
    },
    {
        "sompower-g",
        "\r\nsompower-g: Get the som power status. ON or OFF.\r\n",
        prvCommandSomPwrStatusGet,
        0
    },
    {
        "sompower-s",
        "\r\nsompower-s <1/0>: Power (1)ON or (0)OFF.\r\n",
        prvCommandSomPwrStatusSet,
        1
    },
    {
        "somwork",
        "\r\nsomwork: Get the kernel work status of the som board.\r\n",
        prvCommandSomSwWorkStatusGet,
        0
    },
    {
        "reboot",
        "\r\nreboot: Reboot the kernel on som board.\r\n",
        prvCommandReboot,
        0
    },
    {
        "devmem-r",
        "\r\ndevmem-r <address in hex>: Read the memory/io address of som board.\r\n",
        prvCommandDevmemRead,
        1
    },
    {
        "devmem-w",
        "\r\ndevmem-w <address in hex,like 0x180000000> <value in hex,like 0xabcd1234>: write the memory/io address of som board.\r\n",
        prvCommandDevmemWrite,
        2
    },
    {
       "echo",
       "\r\necho <string to echo>\r\n",
       prvCommandEcho,
       1
    },
    {
        "heap",
        "\r\nheap: Display free heap memory.\r\n",
        prvCommandHeap,
        0
    },
    {
        "ticks",
        "\r\nticks: Display OS tick count and run time in seconds.\r\n",
        prvCommandTicks,
        0
    },
    {
        "version",
        "\r\nversion: Get BMC version\r\n",
        prvCommandBMCVersion,
        0
    },
    { NULL, NULL, NULL, 0 }
};

/**
* @brief Command that gets task statistics.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandTaskStats( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    static uint32_t uTaskIndex = 0;
    static uint32_t uTotalOfTasks = 0;
    static uint32_t uTotalRunTime = 1;
    TaskStatus_t *pxTmpTaskStatus = NULL;
    static TaskStatus_t *pxTaskStatus = NULL;

    if (pxTaskStatus == NULL)
    {
        uTotalOfTasks = uxTaskGetNumberOfTasks();
        pxTaskStatus = pvPortMalloc(uTotalOfTasks * sizeof(TaskStatus_t));
        if (pxTaskStatus == NULL)
        {
           snprintf(pcWriteBuffer, xWriteBufferLen, "Error: Not enough memory for task allocation");
           goto out_cmd_task_stats;
        }
        uTotalOfTasks = uxTaskGetSystemState(pxTaskStatus, uTotalOfTasks, &uTotalRunTime);
        uTaskIndex = 0;
        uTotalRunTime /= 100;
        snprintf(pcWriteBuffer, xWriteBufferLen, prvpcTaskListHeader);
    }
    else
    {
        memset(pcWriteBuffer, 0x00, MAX_OUT_STR_LEN);
        /* Prevent from zero division */
        if (!uTotalRunTime)
        {
            uTotalRunTime = 1;
        }

        pxTmpTaskStatus = &pxTaskStatus[uTaskIndex];
        if (pxTmpTaskStatus->ulRunTimeCounter / uTotalRunTime < 1)
        {
         snprintf(pcWriteBuffer, xWriteBufferLen,
                 "%-16s  %5s  %8lu  %14dB       < 1%%  %11lu\r\n",
                 pxTmpTaskStatus->pcTaskName,
                 prvpcMapTaskState(pxTmpTaskStatus->eCurrentState),
                 pxTmpTaskStatus->uxCurrentPriority,
                 pxTmpTaskStatus->usStackHighWaterMark,
                 pxTmpTaskStatus->ulRunTimeCounter);
        }
        else
        {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "%-16s  %5s  %8lu  %14dB  %8lu%%  %11lu\r\n",
                    pxTmpTaskStatus->pcTaskName,
                    prvpcMapTaskState(pxTmpTaskStatus->eCurrentState),
                    pxTmpTaskStatus->uxCurrentPriority,
                    pxTmpTaskStatus->usStackHighWaterMark,
                    pxTmpTaskStatus->ulRunTimeCounter / uTotalRunTime,
                    pxTmpTaskStatus->ulRunTimeCounter);
        }
        uTaskIndex++;
    }

    /* Check if there is more tasks to be process */
    if (uTaskIndex < uTotalOfTasks)
       return pdTRUE;
    else
    {
out_cmd_task_stats :
        if (pxTaskStatus != NULL)
        {
            vPortFree(pxTaskStatus);
            pxTaskStatus = NULL;
        }
        return pdFALSE;
    }
}


/**
* @brief Command that gets carrier board information.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandCarrierBoardInfoGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    CarrierBoardInfo carrierBoardInfo;
    char *pcWb = pcWriteBuffer;
    size_t len, size = xWriteBufferLen;

    es_get_carrier_borad_info(&carrierBoardInfo);
    len = snprintf(pcWb, size, "[Carrierboard Information:]\r\n");
    pcWb += len;
    size -= len;
    len = snprintf(pcWb, size, "magicNumber:0x%lx\r\n", carrierBoardInfo.magicNumber);
    pcWb += len;
    size -= len;
    len = snprintf(pcWb, size, "formatVersionNumber:0x%x\r\n", carrierBoardInfo.formatVersionNumber);
    pcWb += len;
    size -= len;
    len = snprintf(pcWb, size, "productIdentifier:0x%x\r\n", carrierBoardInfo.productIdentifier);
    pcWb += len;
    size -= len;
    len = snprintf(pcWb, size, "pcbRevision:0x%x\r\n", carrierBoardInfo.pcbRevision);
    pcWb += len;
    size -= len;
    len = snprintf(pcWb, size, "bomRevision:0x%x\r\n", carrierBoardInfo.bomRevision);
    pcWb += len;
    size -= len;
    len = snprintf(pcWb, size, "bomVariant:0x%x\r\n", carrierBoardInfo.bomVariant);
    pcWb += len;
    size -= len;
    len = snprintf(pcWb, size, "SN(hex):");
    pcWb += len;
    size -= len;
    for (int i = 0; i < sizeof(carrierBoardInfo.boardSerialNumber); i++) {
        pcWb += snprintf(pcWb, size, "%x", carrierBoardInfo.boardSerialNumber[i]);
        size--;
    }
    len = snprintf(pcWb, size, "\r\n");
    pcWb += len;
    size -= len;
    snprintf(pcWb, size, "manufacturingTestStatus:%d\r\n", carrierBoardInfo.manufacturingTestStatus);

    return pdFALSE;
}


/**
* @brief Command that gets som board information.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandSomBoardInfoGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    int ret = HAL_OK;
    som_info somInfo;
    som_info *psomInfo = &somInfo;
    char *pcWb = pcWriteBuffer;
    size_t len, size = xWriteBufferLen;

    ret = web_cmd_handle(CMD_READ_BOARD_INFO, psomInfo, sizeof(som_info), 1000);
    if (HAL_OK != ret) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "Faild to get som info(errcode %d)\n", ret);
    }
    else {
        len = snprintf(pcWb, size, "[Somboard Information:]\r\n");
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "magicNumber:0x%lx\r\n", psomInfo->magic);
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "version:0x%x\r\n", psomInfo->version);
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "id:0x%x\r\n", psomInfo->id);
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "pcb:0x%x\r\n", psomInfo->pcb);
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "bom_revision:0x%x\r\n", psomInfo->bom_revision);
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "bom_variant:0x%x\r\n", psomInfo->bom_variant);
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "SN(hex):");
        pcWb += len;
        size -= len;
        for (int i = 0; i < sizeof(psomInfo->sn); i++) {
            pcWb += snprintf(pcWb, size, "%x", psomInfo->sn[i]);
            size--;
        }
        len = snprintf(pcWb, size, "\r\n");
        pcWb += len;
        size -= len;
        len = snprintf(pcWb, size, "status:%d\n", psomInfo->status);
    }

    return pdFALSE;
}

/**
* @brief Command that show account info
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandAccountGet(char *pcWriteBuffer, size_t xWriteBufferLen\
                                     , const char *pcCommandString)
{
    char admin_name[32] = {0};
    char admin_password[32]={0};

    /* Read admin name and password and fill FreeRTOS write buffer */
    es_get_username_password(admin_name, admin_password);

    snprintf(pcWriteBuffer, xWriteBufferLen, "AdminName: %s  Password: %s\r\n",
            admin_name, admin_password);

    return pdFALSE;
}


/**
* @brief Command that set account info
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandAccountSet(char *pcWriteBuffer, size_t xWriteBufferLen\
                                     , const char *pcCommandString)
{
    BaseType_t xParamLen;
    const char * pcAdminName;
    const char * pcAdminPassword;
    char admin_name[32] = {0};
    char admin_password[32]={0};

    pcAdminName = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    /* fill with new accout name */
    strncpy(admin_name, pcAdminName, xParamLen);

    /* Get Admin password parameter */
    pcAdminPassword = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
    strncpy(admin_password, pcAdminPassword, xParamLen);

    /* update the accoutn info finally */
    es_set_username_password(admin_name, pcAdminPassword);

    return pdFALSE;
}


/**
* @brief Command that display the network configuration info
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandNetInfoGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    char *pcWb = pcWriteBuffer;
    size_t len, size = xWriteBufferLen;
    NETInfo netInfo;

    /* Read network information and fill FreeRTOS write buffer */
    netInfo = get_net_info();

    len = snprintf(pcWb, size, "inet %s  netmask: %s\r\n",
                    netInfo.ipaddr, netInfo.subnetwork);
    pcWb += len;
    size -= len;

    len = snprintf(pcWb, size, "gatway %s\r\n", netInfo.gateway);
    pcWb += len;
    size -= len;

    len = snprintf(pcWb, size, "mac %s\r\n", netInfo.macaddr);

    return pdFALSE;
}


/**
* @brief Command that sets a new ip address.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandIPSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
	uint32_t naddr;
    const char * pcIPaddr;
    BaseType_t xParamLen;

    pcIPaddr = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);

	/* set ipaddr */
	naddr = ipaddr_addr(pcIPaddr);
	es_set_mcu_ipaddr((uint8_t *)&naddr);

    snprintf(pcWriteBuffer, xWriteBufferLen, "ip addr set to %s(0x%lx)\r\n", pcIPaddr, naddr);

    return pdFALSE;
}


/**
* @brief Command that sets a new netmask address.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandNetMaskSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
	uint32_t naddr;
    const char * pcIPaddr;
    BaseType_t xParamLen;

    pcIPaddr = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);

	/* set ipaddr */
	naddr = ipaddr_addr(pcIPaddr);
	es_set_mcu_netmask((uint8_t *)&naddr);

    snprintf(pcWriteBuffer, xWriteBufferLen, "netmask addr set to %s(0x%lx)\r\n", pcIPaddr, naddr);

    return pdFALSE;
}


/**
* @brief Command that sets a new gateway address.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandGateWaySet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
	uint32_t naddr;
    const char * pcIPaddr;
    BaseType_t xParamLen;

    pcIPaddr = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);

	/* set ipaddr */
	naddr = ipaddr_addr(pcIPaddr);
	es_set_mcu_gateway((uint8_t *)&naddr);

    snprintf(pcWriteBuffer, xWriteBufferLen, "gateway addr set to %s(0x%lx)\r\n", pcIPaddr, naddr);

    return pdFALSE;
}


/**
* @brief Command that sets mac address.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandMacSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    const char * pcMACaddr;
    BaseType_t xParamLen;
	uint8_t mac[6];

    pcMACaddr = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);

    /* set mac */
    hexstr2mac(mac, pcMACaddr);
    es_set_mcu_mac(mac);

    snprintf(pcWriteBuffer, xWriteBufferLen, "MAC addr set to %s(%x:%x:%x:%x:%x:%x)\r\n",
                pcMACaddr, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return pdFALSE;
}


/**
* @brief Command that show software bootsel configuration
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandDipSwitchSoftGet(char *pcWriteBuffer, size_t xWriteBufferLen\
                                     , const char *pcCommandString)
{
    uint8_t state;
    int ctl_attr;

    /* Read dip switch ctl attribute */
    es_get_som_dip_switch_soft_ctl_attr(&ctl_attr);

    /* Read dip switch state and fill FreeRTOS write buffer */
    es_get_som_dip_switch_soft_state(&state);

    snprintf(pcWriteBuffer, xWriteBufferLen, "Get: Bootsel Controlled by: %s, bootsel[3 2 1 0]:%d %d %d %d\r\n",
            ctl_attr == 1?"SW":"HW", (0x8&state)>>3, (0x4&state)>>2, (0x2&state)>>1, (0x1&state));

    return pdFALSE;

}

/**
* @brief Command that set software bootsel configuration
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandDipSwitchSoftSet(char *pcWriteBuffer, size_t xWriteBufferLen\
                                     , const char *pcCommandString)
{
    BaseType_t xParamLen;
    uint8_t state;
    int ctl_attr;
    const char * pcCtlAttr;
    const char * pcState;
    int setAttrFlag = 0;

    pcCtlAttr = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    if ((0 == strncmp("sw", pcCtlAttr, xParamLen)) || (0 == strncmp("SW", pcCtlAttr, xParamLen))) {
        ctl_attr = 1;
        setAttrFlag = 1;
    }
    else if ((0 == strncmp("hw", pcCtlAttr, xParamLen)) || (0 == strncmp("HW", pcCtlAttr, xParamLen))){
        ctl_attr = 0;
        setAttrFlag = 1;
    }
    else {
        snprintf(pcWriteBuffer, xWriteBufferLen, "Invalid parameter!\r\n");
    }

    if (setAttrFlag) {
        /* set new dip switch ctl attr */
        es_set_som_dip_switch_soft_ctl_attr(ctl_attr);

        /* get dip switch state parameter */
        pcState = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
        state = atoh(pcState, xParamLen) & 0xF;
        /* set new dip switch state */
        es_set_som_dip_switch_soft_state(state);

        snprintf(pcWriteBuffer, xWriteBufferLen, "Set: Bootsel Controlled by: %s, bootsel[3 2 1 0]:%d %d %d %d\r\n",
                ctl_attr == 1?"SW":"HW", (0x8&state)>>3, (0x4&state)>>2, (0x2&state)>>1, (0x1&state));
    }

    return pdFALSE;
}

/**
* @brief Echo command line in UNIX systems.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandEcho( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    const char *pcStrToOutput;
    BaseType_t xParamLen;

    /* Get the user input and write it back the FreeRTOS write buffer */
    pcStrToOutput = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    snprintf(pcWriteBuffer, xWriteBufferLen, "%s\n", pcStrToOutput);

    return pdFALSE;
}

/**
* @brief Command that gets heap information
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandHeap(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{

    size_t xHeapFree;
    size_t xHeapMinMemExisted;

    xHeapFree = xPortGetFreeHeapSize();
    xHeapMinMemExisted = xPortGetMinimumEverFreeHeapSize();
    snprintf(pcWriteBuffer, xWriteBufferLen,
             "Heap size            : %3u bytes (%3d KiB)\r\nRemaining            : %3u bytes (%3d KiB)\r\nMinimum ever existed : %3u bytes (%3d KiB)\r\n",
             configTOTAL_HEAP_SIZE, configTOTAL_HEAP_SIZE / 1024, xHeapFree, xHeapFree / 1024, xHeapMinMemExisted, xHeapMinMemExisted / 1024);

    return pdFALSE;
}

/**
* @brief Command that calculate OS ticks information.
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandTicks(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    uint32_t uMs;
    uint32_t uSec;
    TickType_t xTickCount = xTaskGetTickCount();

    uSec = xTickCount / configTICK_RATE_HZ;
    uMs = xTickCount % configTICK_RATE_HZ;
    snprintf(pcWriteBuffer, xWriteBufferLen,
             "Tick rate: %u Hz\r\nTicks: %lu\r\nRun time: %lu.%.3lu seconds\r\n",
              (unsigned)configTICK_RATE_HZ, xTickCount, uSec, uMs);

    return pdFALSE;
}


/**
* @brief Get the current time stored in RTC registers
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandRtcGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
	struct rtc_date_t date = {0};
	struct rtc_time_t time = {0};
    char cWeekDay[4] = {0};

	es_get_rtc_date(&date);
	es_get_rtc_time(&time);

    switch (date.WeekDay)
    {
        case 1:
            strncpy(cWeekDay, "Mon", 4);
            break;
        case 2:
            strncpy(cWeekDay, "Tue", 4);
            break;
        case 3:
            strncpy(cWeekDay, "Wed", 4);
            break;
        case 4:
            strncpy(cWeekDay, "Thu", 4);
            break;
        case 5:
            strncpy(cWeekDay, "Fri", 4);
            break;
        case 6:
            strncpy(cWeekDay, "Sat", 4);
            break;
        case 7:
            strncpy(cWeekDay, "Sun", 4);
            break;
        default:
            break;
    }
    snprintf(pcWriteBuffer, xWriteBufferLen, "%u-%u-%u %s %u:%u:%u CST\r\n",
                date.Year, date.Month, date.Date, cWeekDay,
                time.Hours, time.Minutes, time.Seconds);

    return pdFALSE;
}

/**
* @brief Set a new date in RCT registers
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandRtcDateSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    const char *cYear;
    const char *cMonth;
    const char *cDate;
    const char *cWeekDay;
    BaseType_t xParamLen;
    struct rtc_date_t date = {0};

    cYear = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    cMonth = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
    cDate = FreeRTOS_CLIGetParameter(pcCommandString, 3, &xParamLen);
    cWeekDay = FreeRTOS_CLIGetParameter(pcCommandString, 4, &xParamLen);

    date.Year = atoi(cYear);
    date.Month = atoi(cMonth);
    date.Date = atoi(cDate);
    date.WeekDay = atoi(cWeekDay);

    /* update time */
    es_set_rtc_date(&date);
    snprintf(pcWriteBuffer, xWriteBufferLen, "Date set to: %u-%u-%u WeekDay %d\n",
            date.Year, date.Month, date.Date, date.WeekDay);

    return pdFALSE;
}


/**
* @brief Set a new time in RCT registers
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandRtcTimeSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    const char *cHours;
    const char *cMinutes;
    const char *cSeconds;
    BaseType_t xParamLen;
    struct rtc_time_t time = {0};

    cHours = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    cMinutes = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
    cSeconds = FreeRTOS_CLIGetParameter(pcCommandString, 3, &xParamLen);

    time.Hours = atoi(cHours);
    time.Minutes = atoi(cMinutes);
    time.Seconds = atoi(cSeconds);

    /* update time */
    es_set_rtc_time(&time);
    snprintf(pcWriteBuffer, xWriteBufferLen, "Time (24hr format) set to: %u:%u:%u\n",
            time.Hours, time.Minutes, time.Seconds);

    return pdFALSE;
}


/**
* @brief Show temperature and fan speed
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandTempGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    int ret = HAL_OK;
    PVTInfo pvtInfo;

    ret = web_cmd_handle(CMD_PVT_INFO, &pvtInfo, sizeof(PVTInfo), 1000);
    if (HAL_OK != ret) {
         snprintf(pcWriteBuffer, xWriteBufferLen, "Faild to get PVT info(errcode:%d)\n", ret);
    }
    else {
        snprintf(pcWriteBuffer, xWriteBufferLen,"cpu_temp:%d  npu_temp:%d  fan_speed:%d\n",
            pvtInfo.cpu_temp, pvtInfo.npu_temp, pvtInfo.fan_speed);
    }

    return pdFALSE;
}


/**
* @brief Show power consumption(including current and voltage)
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandPwrDissipationGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    uint32_t millivolt = 0, milliCur = 0, microWatt = 0; //millivolt, milliCurrent, milliwatt
    int pwrStaus;

    pwrStaus = get_som_power_state();
    if (pwrStaus == SOM_POWER_ON) {
        get_board_power(&millivolt, &milliCur, &microWatt);
    }
    snprintf(pcWriteBuffer, xWriteBufferLen,"consumption:%ld.%3ld(W)  voltage:%ld.%3ld(V)  current:%ld.%3ld(A)\n",
        microWatt / 1000000, microWatt % 1000000, millivolt / 1000, millivolt % 1000, milliCur / 1000, milliCur % 1000);

    return pdFALSE;
}


/**
* @brief Get the som power status: ON or OFF
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandSomPwrStatusGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    int pwrStaus;

    pwrStaus = get_som_power_state();
    snprintf(pcWriteBuffer, xWriteBufferLen, "Som Power Status: %s\n", (pwrStaus == SOM_POWER_ON)?"ON":"OFF");

    return pdFALSE;
}


/**
* @brief Power On or OFF the som board
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandSomPwrStatusSet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    int ret = 0;
    const char *cPwrOnOff;
    BaseType_t xParamLen;
    uint8_t powerOnOff;

    cPwrOnOff = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    powerOnOff = atoi(cPwrOnOff);

    /* change som power */
    if (0 == powerOnOff) {
        if (SOM_POWER_ON == get_som_power_state()) {
            ret = web_cmd_handle(CMD_POWER_OFF, NULL, 0, 1000);
            if (HAL_OK != ret) {
                change_som_power_state(SOM_POWER_OFF);
                printf("Poweroff SOM error(ret %d), force shutdown it!\n", ret);
            }
            // Trigger the Som timer to enusre SOM could poweroff in 5 senconds
            TriggerSomPowerOffTimer();
        } else {
            printf("SOM already power off!\n");
        }
    } else {
        if (SOM_POWER_OFF == get_som_power_state()) {
            change_som_power_state(SOM_POWER_ON);
        } else {
            printf("SOM already power on!\n");
        }
    }

    return pdFALSE;
}


/**
* @brief get the system runnig status of the som board, running or stoppted
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandSomSwWorkStatusGet(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    int workStaus;

    workStaus = get_som_daemon_state();
    snprintf(pcWriteBuffer, xWriteBufferLen, "Som Work Status: %s\n", (workStaus == SOM_DAEMON_ON)?"Running":"Stopped");

    return pdFALSE;
}

/**
* @brief reboot the kernel on the som
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandReboot(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    int ret = HAL_OK;

    ret = web_cmd_handle(CMD_RESET, NULL, 0, 1000);
    if (HAL_OK != ret) {
        som_reset_control(pdTRUE);
        osDelay(10);
        som_reset_control(pdFALSE);
        printf("Faild to reboot SOM(ret %d), force reset SOM!\n", ret);
    }
    // Trigger the Som timer to enusre SOM could reboot in 5 senconds
    TriggerSomRebootTimer();

    return pdFALSE;
}


/**
* @brief Get current console version
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandBMCVersion(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    snprintf(pcWriteBuffer, xWriteBufferLen, "%d.%d\n", (uint8_t)(BMC_SOFTWARE_VERSION_MAJOR), (uint8_t)(BMC_SOFTWARE_VERSION_MINOR));
    return pdFALSE;
}

/**
* @brief Command that read 4 bytes of the memory/io on som
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandDevmemRead(char *pcWriteBuffer, size_t xWriteBufferLen\
                                     , const char *pcCommandString)
{
    const char *cAddr;
    BaseType_t xParamLen;
    uint64_t memAddr;
    uint32_t value;
    uint32_t memAddrLow, memAddrHigh;
    // uint8_t outbuf[4];

    /* get the memory/io address, in hex */
    cAddr = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    memAddr = atoh(cAddr, xParamLen);
    memAddrHigh = 0xFFFFFFFF & (memAddr >> 32);
    memAddrLow = 0xFFFFFFFF & memAddr;

    /* Read memory/io and fill FreeRTOS write buffer */
    if (HAL_OK != es_spi_read((uint8_t *)&value, memAddr, 4)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "Failed to read mem/io at 0x%llx\n", memAddr);
        goto out;
    }
    // value = ((uint32_t)outbuf[3] << 24) | ((uint32_t)outbuf[2] << 16) | ((uint32_t)outbuf[1] << 8) | (uint32_t)outbuf[0];
    snprintf(pcWriteBuffer, xWriteBufferLen, "addr:0x%lX%lX  value:0x%08lX\n", memAddrHigh, memAddrLow, value);

out:
    return pdFALSE;
}

/**
* @brief Command that write 4 bytes of the memory/io on som
* @param *pcWriteBuffer FreeRTOS CLI write buffer.
* @param xWriteBufferLen Length of write buffer.
* @param *pcCommandString pointer to the command name.
* @retval FreeRTOS status
*/
static BaseType_t prvCommandDevmemWrite(char *pcWriteBuffer, size_t xWriteBufferLen\
                                     , const char *pcCommandString)
{
    const char *cAddr;
    const char *cValue;
    BaseType_t xParamLen;
    uint64_t memAddr;
    uint32_t value;
    uint32_t memAddrLow, memAddrHigh;

    /* get the memory/io address, in hex */
    cAddr = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParamLen);
    memAddr = atoh(cAddr, xParamLen);
    memAddrHigh = 0xFFFFFFFF & (memAddr >> 32);
    memAddrLow = 0xFFFFFFFF & memAddr;

    cValue = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParamLen);
    value = atoh(cValue, xParamLen);

    /* Write memory/io and fill FreeRTOS write buffer */
    if (HAL_OK != es_spi_write((uint8_t *)&value, memAddr, 4)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "Failed to write mem/io at 0x%lx%lx\n", memAddrHigh, memAddrLow);
    }

    return pdFALSE;
}

/**
* @brief Reads from UART RX buffer. Reads one bye at the time.
* @param *cReadChar pointer to where data will be stored.
* @retval FreeRTOS status
*/
static BaseType_t xConsoleRead(uint8_t *cReadChar, size_t xLen)
{
    BaseType_t xRetVal = pdFALSE;

    if (xQueueRxHandle == NULL || cReadChar == NULL)
    {
        return xRetVal;
    }

    /* Block until the there is input from the user */
    return xQueueReceive(xQueueRxHandle, cReadChar, portMAX_DELAY);
}

/**
* @brief Write to UART TX
* @param *buff buffer to be written.
* @retval HAL status
*/
static HAL_StatusTypeDef vConsoleWrite(const char *buff)
{
    HAL_StatusTypeDef status;
    size_t len = strlen(buff);

    if (pxUartDevHandle == NULL || *buff == '\0' || len < 1)
    {
        return HAL_ERROR;
    }

    status = HAL_UART_Transmit(pxUartDevHandle, (uint8_t *)buff, strlen(buff), portMAX_DELAY);
    if (status != HAL_OK)
    {
    	return HAL_ERROR;
    }
    return status;
}

/**
* @brief Enables UART RX reception.
* @param void
* @retval void
*/
void vConsoleEnableRxInterrupt(void)
{
    if (pxUartDevHandle == NULL)
    {
        return;
    }
    /* UART Rx IT is enabled by reading a character */
    HAL_UART_Receive_IT(pxUartDevHandle,(uint8_t*)&cRxData, 1);
}

/**
* @brief Task to handle user commands via serial communication.
* @param *pvParams Data passed at task creation.
* @retval void
*/
void vTaskConsole(void *pvParams)
{
    char cReadCh = '\0';
    uint8_t uInputIndex = 0;
    BaseType_t xMoreDataToProcess;
    char pcInputString[MAX_IN_STR_LEN];
    char pcPrevInputString[MAX_IN_STR_LEN];
    char pcOutputString[MAX_OUT_STR_LEN];

    memset(pcInputString, 0x00, MAX_IN_STR_LEN);
    memset(pcPrevInputString, 0x00, MAX_IN_STR_LEN);
    memset(pcOutputString, 0x00, MAX_OUT_STR_LEN);

    /* Create a queue to store characters from RX ISR */
    xQueueRxHandle = xQueueCreate(MAX_RX_QUEUE_LEN, sizeof(char));
    if (xQueueRxHandle == NULL)
    {
        goto out_task_console;
    }

    vConsoleWrite(pcWelcomeMsg);
    vConsoleEnableRxInterrupt();
    vConsoleWrite(prvpcPrompt);

    while(1)
    {
        /* Block until there is a new character in RX buffer */
        xConsoleRead((uint8_t*)(&cReadCh), sizeof(cReadCh));

        switch (cReadCh)
        {
            case ASCII_CR:
            case ASCII_LF:
                if (uInputIndex != 0)
                {
                    vConsoleWrite("\r\n");
                    strncpy(pcPrevInputString, pcInputString, MAX_IN_STR_LEN);
                    do
                    {
                        xMoreDataToProcess = FreeRTOS_CLIProcessCommand
                                            (
                                                pcInputString,    /* Command string*/
                                                pcOutputString,   /* Output buffer */
                                                MAX_OUT_STR_LEN   /* Output buffer size */
                                            );
                        vConsoleWrite(pcOutputString);
                    } while (xMoreDataToProcess != pdFALSE);
                }
                uInputIndex = 0;
                memset(pcInputString, 0x00, MAX_IN_STR_LEN);
                memset(pcOutputString, 0x00, MAX_OUT_STR_LEN);
                vConsoleWrite("\r\n");
                vConsoleWrite(prvpcPrompt);
                break;
            case ASCII_FORM_FEED:
                vConsoleWrite("\x1b[2J\x1b[0;0H");
                vConsoleWrite("\n");
                vConsoleWrite(prvpcPrompt);
                break;
            case ASCII_CTRL_PLUS_C:
                uInputIndex = 0;
                memset(pcInputString, 0x00, MAX_IN_STR_LEN);
                vConsoleWrite("\n");
                vConsoleWrite(prvpcPrompt);
                break;
            case ASCII_DEL:
            case ASCII_NACK:
            case ASCII_BACKSPACE:
                if (uInputIndex > 0)
                {
                    uInputIndex--;
                    pcInputString[uInputIndex] = '\0';
                    vConsoleWrite("\b \b");
                }
                break;
            case ASCII_TAB:
                while (uInputIndex)
                {
                    uInputIndex--;
                    vConsoleWrite("\b \b");
                }
                strncpy(pcInputString, pcPrevInputString, MAX_IN_STR_LEN);
                uInputIndex = (unsigned char)strlen(pcInputString);
                vConsoleWrite(pcInputString);
                break;
            default:
                /* Check if read character is between [Space] and [~] in ASCII table */
                if (uInputIndex < (MAX_IN_STR_LEN - 1 ) && (cReadCh >= 32 && cReadCh <= 126))
                {
                    pcInputString[uInputIndex] = cReadCh;
                    vConsoleWrite(pcInputString + uInputIndex);
                    uInputIndex++;
                }
                break;
        }
    }

out_task_console:
    if (xQueueRxHandle)
    {
        vQueueDelete(xQueueRxHandle);
    }
    vTaskDelete(NULL);
}

/**
* @brief Initialize the console by registering all commands and creating a task.
* @param usStackSize Task console stack size
* @param uxPriority Task console priority
* @param *pxUartHandle Pointer for uart handle.
* @retval FreeRTOS status
*/
BaseType_t xbspConsoleInit(uint16_t usStackSize, UBaseType_t uxPriority, UART_HandleTypeDef *pxUartHandle)
{
    const CLI_Command_Definition_t *pCommand;

    if (pxUartHandle == NULL)
    {
        return pdFALSE;
    }
    pxUartDevHandle = pxUartHandle;

    /* Register all commands that can be accessed by the user */
    for (pCommand = xCommands; pCommand->pcCommand != NULL; pCommand++)
    {
        FreeRTOS_CLIRegisterCommand(pCommand);
    }
    return xTaskCreate(vTaskConsole,"CLI", usStackSize, NULL, uxPriority, NULL);
}

#if !ES_PRODUCTION_LINE_TEST
/**
* @brief Callback for UART RX, triggered any time there is a new character.
* @param *huart Pointer to the uart handle.
* @retval void
*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

    if (xQueueRxHandle != NULL)
    {
        xQueueSendToBackFromISR(xQueueRxHandle, &cRxData, &pxHigherPriorityTaskWoken);
    }
    vConsoleEnableRxInterrupt();
}
#endif