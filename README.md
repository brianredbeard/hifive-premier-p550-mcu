These are the source code of STM32 MCU for HF106, proved as patch files.
STM32 MCU is working as BMC(Baseboard Manager Contoller) on HF106C board.
The HiFive Premier P550 (HF106) is a development platform based on ESWIN's
EIC7700 Edge Computing SoC integrated with SiFive’s P550 cores.
It consists of a SOM (System on Module) (HF106S) and a Carrier (HF106C) board.

The steps for creating MCU software:
1. Downloading STM32 CubeMX tool from link:
   https://www.st.com/content/st_com/en/stm32cubemx.html

   Notes: The software versions must be 6.10.0 on Linux operating system
   because the patches are generated based on this version. 

2. Generating the source code of the base project through STM32 Cubemx tool by
   loading the provided STM32F407VET6_BMC.ioc file.

3. Replacing the Makefile file in the root directory of the generated source code
   with the provided Makefile file.
   
   Notes:This is because the arrangement order of the files in the Makefile may
   vary each time the source code is generated. The Makefile conflict may
   happen while applying patches.

4. Applying patches
   Run "git am --ignore-space-change --ignore-whitespace patches-yyyy-mm-dd/*.patch"
   to apply all the patches under patches-yyyy-mm-dd folders.

   Please apply the patches in the chronological order of the patch folders.
   The first patch folder is named patches-2025-11-05.

5. Building the project
   a. Downloading Arm GNU Toolchain from link:
      https://developer.arm.com/downloads/-/gnu-rm
   b. Changing the GCC_PATH in the Makefile to the real path of the Toolchain.
   c. Building project: Run "make", the STM32F407VET6_BMC.elf file will be
      generated under the build file folder.

6. References:
   Refer to https://www.sifive.com/document-file/premier-p550-mcu-user-manual
   for the details of software functionality and how to flash the STM32F407VET6_BMC.elf
