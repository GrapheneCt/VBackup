# VBackup
Advanced backup tool for PS Vita. Creates backups of applications and savedata by packing them in filesystem image that can be saved on any of the storage devices available on your PS Vita.

# Features
- Full application backup
- Savedata-only backup
- Optional compression of backup data
- Backups can be opened on PC
- Supports ux0:, ur0:, uma0:, imc0:, host0:, sd0: as backup locations
- Supports quick backups from predefined list of applications

# Backup From List feature
- To enable feature, create list of target application titleids at ```ur0:vbackup_list.txt```. Example of formatting:
```
PCSE00001
PCSE00002
PCSE00003
```

# Adding new language:

- Use [English locale](https://github.com/GrapheneCt/VBackup/blob/main/VBackup/RES_RCO/locale/vbackup_locale_en.xml) as base. Name your translation vbackup_locale_XX.xml, where XX is language code.
Supported language codes can be seen [here](https://github.com/GrapheneCt/VBackup/blob/main/VBackup/RES_RCO/vbackup_plugin.xml#L159) (id attribute).
- Submit your translation as pull request, it will be added in the next release.

# Credits
[Princess-of-Sleeping](https://github.com/Princess-of-Sleeping) - application idea, backup_core development

[VitaShell](https://github.com/TheOfficialFloW/VitaShell) - SFO parser
