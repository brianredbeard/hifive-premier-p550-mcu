# BMC Firmware Patch Documentation

This directory contains comprehensive documentation for the HiFive Premier P550 BMC firmware patch series.

## Documentation Index

### Critical Patches (Detailed Documentation)

**Patch 0079: SiFive CRC32 Algorithm** - [0079-CRC32-SiFive-Algorithm-CRITICAL.md](0079-CRC32-SiFive-Algorithm-CRITICAL.md)
- Replaces hardware CRC with software CRC32
- Ensures cross-platform compatibility with SiFive P550
- Breaking change: invalidates all existing EEPROM data
- **Criticality**: CRITICAL

**Patch 0080: EEPROM Write Protection** - [0080-EEPROM-Write-Protection.md](0080-EEPROM-Write-Protection.md)
- Adds production-line write protection commands
- Hardware WP pin control
- Power state diagnostic commands
- **Criticality**: HIGH

### Patch Series Overview

**Patches 0071-0080 Summary** - [PATCHES-0071-0080-SUMMARY.md](PATCHES-0071-0080-SUMMARY.md)
- Complete overview of all 10 patches
- Breaking changes summary
- Migration checklist
- Performance impact analysis
- Testing matrix

## Additional Documentation

### Previously Documented Patches

See the docs/patches directory for additional detailed documentation:

- **0001**: First version
- **0002**: Protocol lib and production test
- **0003**: Production test bug fix
- **0004**: Production test bug fixes
- **0008**: Add feature
- **0013**: SOM CB info fix bug
- **0014**: BMC web cmd support
- **0018**: BMC web cmd support
- **0019**: Power sequence optimization
- **0020**: Web cmd support poweroff
- **0021**: SocStatus PowerStatus bug fixes
- **0022**: EEPROM callback integration
- **0030**: INA226 power monitoring (CRITICAL)
- **0033**: CLI console UART3
- **0040**: Login AJAX sync
- **0041**: Cbinfo s command
- **0047**: I2C status not ready (CRITICAL)
- **0050**: Memory optimization pvPortMalloc

## Documentation Format

Each detailed patch documentation follows this structure:

1. **Metadata**: Author, date, ticket, type, criticality
2. **Summary**: High-level overview of changes
3. **Changelog**: Bullet points from commit message
4. **Statistics**: File changes, line counts
5. **Detailed Analysis**: Code-level implementation details
6. **Testing**: Test procedures and validation
7. **Security Implications**: Vulnerability analysis
8. **Related Patches**: Dependencies and connections
9. **Conclusion**: Production readiness assessment

## Quick Reference

### By Criticality

**CRITICAL**:
- 0030: INA226 Power Monitoring
- 0047: I2C Status Not Ready
- 0071: A/B Partition Support
- 0075: DVB2 Board Support
- 0079: SiFive CRC32 Algorithm

**HIGH**:
- 0077: MAC Configuration Security
- 0080: EEPROM Write Protection

**MEDIUM**:
- 0072: Web Server Restart
- 0074: Console Power Commands

**LOW**:
- 0073: UART RX DMA→IT Fix
- 0076: PWM LED Control

### By Topic

**Power Management**:
- 0019: Power sequence optimization
- 0020: Web poweroff support
- 0071: Power loss resume
- 0072: Reboot/restart
- 0074: Console power commands

**EEPROM/Storage**:
- 0022: EEPROM callback
- 0071: A/B partition system
- 0079: CRC32 algorithm
- 0080: Write protection

**Hardware**:
- 0030: INA226 sensor integration
- 0047: I2C reliability
- 0075: DVB2 board support
- 0076: PWM LED control

**Web Interface**:
- 0014: Web command support
- 0018: Additional web commands
- 0040: Login AJAX sync
- 0072: Restart functionality
- 0077: MAC security

**Console/CLI**:
- 0033: UART3 console
- 0041: Cbinfo command
- 0074: Power command fixes
- 0080: Production commands

## Reading Guide

### For Developers

**Start Here**:
1. PATCHES-0071-0080-SUMMARY.md (overview)
2. 0079-CRC32-SiFive-Algorithm-CRITICAL.md (core algorithm)
3. 0071 documentation (when created) for A/B partitions
4. 0030-INA226-Power-Monitoring-CRITICAL.md (hardware integration)

**Development Workflow**:
- Check patch dependencies before implementing changes
- Review security implications for user-facing features
- Follow testing procedures before committing

### For Security Researchers

**Key Documents**:
1. 0080-EEPROM-Write-Protection.md (production security)
2. PATCHES-0071-0080-SUMMARY.md (attack surface analysis)
3. 0079-CRC32-SiFive-Algorithm-CRITICAL.md (tamper detection limitations)

**Security Context**:
- Authorized research project with completed disclosure
- Full vulnerability analysis support provided
- Defensive analysis encouraged

### For Production Engineers

**Essential Reading**:
1. PATCHES-0071-0080-SUMMARY.md (migration checklist)
2. 0080-EEPROM-Write-Protection.md (production commands)
3. 0079-CRC32-SiFive-Algorithm-CRITICAL.md (data integrity)

**Production Workflow**:
- Follow migration checklist exactly
- Test A/B partition recovery
- Enable write protection before shipping

## Contributing

When documenting new patches, follow the established format:

```markdown
# Patch XXXX: Title

## Metadata
- Patch File
- Author
- Date
- Ticket
- Type
- Criticality

## Summary
[High-level overview]

## Changelog
[From commit message]

## Statistics
[File counts, line changes]

## Detailed Implementation Analysis
[Section by section code analysis]

## Testing and Validation
[Test procedures]

## Security Implications
[Vulnerability analysis]

## Related Patches
[Dependencies]

## Conclusion
[Assessment]
```

## Tools and Scripts

### Patch Analysis

```bash
# List all patches in order
ls patches-2025-11-05/*.patch | sort

# View patch metadata
head -20 patches-2025-11-05/0079-*.patch

# Search for specific changes
grep -r "sifive_crc32" patches-2025-11-05/

# Analyze patch size
for p in patches-2025-11-05/*.patch; do
    echo -n "$(basename $p): "
    wc -l < "$p"
done | sort -t: -k2 -n
```

### Documentation Status

```bash
# Check documented patches
ls docs/patches/*.md | grep -E "^[0-9]{4}" | wc -l

# Find undocumented patches
comm -23 \
    <(ls patches-2025-11-05/*.patch | sed 's/.*\/\([0-9]*\)-.*/\1/' | sort) \
    <(ls docs/patches/*.md | grep -oE "^[0-9]{4}" | sort)
```

## Version History

- **2025-11-24**: Created comprehensive documentation for patches 0079-0080
- **2025-11-24**: Added PATCHES-0071-0080-SUMMARY.md overview
- **[Earlier]**: Documentation for patches 0001-0050

## Contact

For questions about patch documentation:
- Review CLAUDE.md in repository root
- Check existing documentation for similar patches
- Follow established documentation patterns

---

**Last Updated**: 2025-11-24
**Documentation Coverage**: Patches 0001-0080 (selective detailed coverage)
