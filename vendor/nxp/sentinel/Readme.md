# Revision History

# **ELE FW NOM v0.0.9**

# Release description

This is the release note for EdgeLock Enclave (ELE) Firmware. This ELE FW is authenticated and installed by ELE ROM. It provides new features, fixes, and exposes cryptographic services to other cores through SAB.

## Existing feature changes 
* Open GP fuses to read fuse API (8ULP  only)

## New features
* Service swap API (8ULP only)
* Get info API version 2
* SAB functionalities are now available after RTD powerdown
* Generic API: RSA key generate, Sign/Verify and Encrypt/Decrypt (RT1180 only)

## Bug fixes
RC1 bug fixes:
* OEM RSA PSS authenticate container now use hash lenght as saltLen. Used to be 20. (93 only)
* Get info API fix
RC2 (RT1180 only):
* Get info API back to version 1
* Data storage abort bug fix

# Delivery contents

## Supported revsion
* i.MX93 rev A0
* i.MX8ULP rev A1
* i.MXRT1180 rev A0

## Release artifacts

The release is composed of the following artifacts:

* Sentinel documentation
[ELE Baseline API](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/Architecture/ELE%20Baseline%20API.docx?d=wdd4d3179a95746c4a21c60ecff8ad0d0&csf=1&web=1)

* SAB main documentation
[S40x Sentinel API Bridge detailed implementation](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/HSM/S40x%20Sentinel%20API%20Bridge%20detailed%20implementation.docx?d=wb05d7726aa5447b39cb14c768503ecf3&csf=1&web=1&e=fuo5MP)

## FW container location

ELE FW container v0.0.9 can be found here :
  
- [mx[SOC][rev][alt]-ahab-container.img](https://bitbucket.sw.nxp.com/projects/MCUSEC/repos/sentinel-fw-release/browse)

## Static code analysis

Static code analysis tool used is Coverity central analysis version **2021.12.1**

## Build instructions

The FW cannot be rebuilt and is available in binary form only.


# **ELE FW NOM v0.0.8**

# Release description

This is the release note for EdgeLock Enclave (ELE) Firmware. This ELE FW is authenticated and installed by ELE ROM. It provides new features, fixes, and exposes cryptographic services to other cores through SAB.

## Existing feature changes 
* SAB Improve error code indication
* SAB Get random API don't need open/close anymore
* SAB Do not abort anymore in case of a NVM failure
* SAB Pub key recovery API update (key type not needed anymore)
* SAB Fix signature API and add SAB_ELE_ALG_ECDSA_ANY signature scheme
* SAB Update key generate API
* SAB Maximum global chunk is now 100
* SAB Data storage is no more limited to 1024 bytes
* SAB Bugfix and internal improvments
  
## New features
* SAB Get key attributes API
* Derive key API

## Bug fixes
RC1 bug fixes:
* OEM blob can now be generated in DDR.
* CAAM is now released in secure state in OEM closed lifecycles (8ULP)
* SAB AEAD API fix
  
RC2 bug fixes:
* 8ULP: XRDC release with config 5
* 8ULP: Handle APD power-up in case S400 is the only DBD owner
* SAB: Fix SRDC chunk import/export
* SAB: Fix transient keystore closing
* SAB: fix cipher for size bigger than UINT16_MAX
* Allow write secure fuse mass updates in the open lifecycles
* Secure counter isr bug fix
* OEM Encryption fix for image size bigger than 65k

Hotfix 1(8ULP A1 Only):
* Fuse bank 32 to 36 and 49 to 51 can be read through ELE read fuse API
* Hotfix bit in FW version increased

# Delivery contents

## Supported revsion
* i.MX93 rev A0
* i.MX8ULP rev A1
* i.MX8ULP rev A0

## Release artifacts

The release is composed of the following artifacts:

* Sentinel documentation
[ELE Baseline API](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/Architecture/ELE%20Baseline%20API.docx?d=wdd4d3179a95746c4a21c60ecff8ad0d0&csf=1&web=1)

* SAB main documentation
[S40x Sentinel API Bridge detailed implementation](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/HSM/S40x%20Sentinel%20API%20Bridge%20detailed%20implementation.docx?d=wb05d7726aa5447b39cb14c768503ecf3&csf=1&web=1&e=fuo5MP)

## FW container location

ELE FW container v0.0.8 can be found here :
  
- [mx[SOC][rev][alt]-ahab-container.img](https://bitbucket.sw.nxp.com/projects/MCUSEC/repos/sentinel-fw-release/browse)

## Static code analysis

Static code analysis tool used is Coverity central analysis version **2021.12.1**

## Build instructions

The FW cannot be rebuilt and is available in binary form only.



# **ELE FW NOM v0.0.7**

# Release description

This is the release note for EdgeLock Enclave (ELE) Firmware. This ELE FW is authenticated and installed by ELE ROM. It provides new features, fixes, and exposes cryptographic services to other cores through SAB.

## Existing feature changes 
* SAB Improve RNG API performance 
* SAB Improve verify API performance
* SAB AEAD GCM IV can now be fully generated by the user 
* SAB AEAD no more payload size limitation
* SAB key delete do not require key_group in parameter anymore
  
## New features
* SAB Init API (will be needed later)
* SAB Min mac length now suported
* RT1180: Data storage added 

## Bug fixes
RC2 bug fix (8ULP A1 only) : 
* TRDC fix 
* Mandatory HSM API are now accessible after a power down

# Delivery contents

## Supported revsion
* i.MX93 rev A0
* i.MXRT1180 rev A0
* i.MX8ULP rev A0
* i.MX8ULP rev A1

## Known limitation 

## Release artifacts

The release is composed of the following artifacts:

* Sentinel documentation
[ELE Baseline API](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/Architecture/ELE%20Baseline%20API.docx?d=wdd4d3179a95746c4a21c60ecff8ad0d0&csf=1&web=1)

* SAB main documentation
[S40x Sentinel API Bridge detailed implementation](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/HSM/S40x%20Sentinel%20API%20Bridge%20detailed%20implementation.docx?d=wb05d7726aa5447b39cb14c768503ecf3&csf=1&web=1&e=fuo5MP)

## FW container location

ELE FW container v0.0.7 can be found here :
  
- [mx[SOC][rev][alt]-ahab-container.img](https://bitbucket.sw.nxp.com/projects/MCUSEC/repos/sentinel-fw-release/browse)


## Tests description

Functional tests have been performed on the SAB features delivered:

**SAB :**
* Asymetric NIST/BRAINPOOL cypto signature/verify
* Hash 256/384/512
* SAB Key management
* Symetric crypto AES 128/256 CBC/ECB/CCM
* CMAC/HMAC
* RNG 
* Generic crypto
* Data storage

## Static code analysis

Static code analysis tool used is Coverity central analysis version **2021.12.1**

## Build instructions

The FW cannot be rebuilt and is available in binary form only.



# **ELE FW NOM v0.0.6**

# Release description

This is the release note for EdgeLock Enclave (ELE) Firmware. This ELE FW is authenticated and installed by ELE ROM. It provides new features, fixes, and exposes cryptographic services to other cores through SAB.

## Existing feature changes 
* Get FW version API rework
* SAB Success status is now 0xd6
* SAB Generic crypto API command id has changed 
* SAB cipher is no more limited to 1024 bytes
* SAB Payload for MAC is no more limited 
* SAB rework of some algorithm define

## New features
* OEM Image encryption
* TRNG start API
* TRNG state API
* Debug Authentication

## Bug fixes

NA

# Delivery contents

## Supported revsion
* i.MXRT1180 rev A0
* i.MX93 rev A0

## Known limitation 
* iMX93: FW will not authenticate in OEM closed lifecycles as it is an engineering release

## Release artifacts

The release is composed of the following artifacts:

* Sentinel documentation
[ELE Baseline API](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/Architecture/ELE%20Baseline%20API.docx?d=wdd4d3179a95746c4a21c60ecff8ad0d0&csf=1&web=1)

* SAB main documentation
[S40x Sentinel API Bridge detailed implementation](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/HSM/S40x%20Sentinel%20API%20Bridge%20detailed%20implementation.docx?d=wb05d7726aa5447b39cb14c768503ecf3&csf=1&web=1&e=fuo5MP)

## FW container location

ELE FW container v0.0.6 can be found here :
  
- [mx[SOC]a0-ahab-container.img](https://bitbucket.sw.nxp.com/projects/MCUSEC/repos/sentinel-fw-release/browse)

- [mxrt1180a0alt-ahab-container.img](https://bitbucket.sw.nxp.com/projects/MCUSEC/repos/sentinel-fw-release/browse)

## Tests description

Functional tests have been performed on the SAB features delivered:

**SAB :**
* Asymetric NIST/BRAINPOOL cypto signature/verify
* Hash 256/384/512
* SAB Key management
* Symetric crypto AES 128/256 CBC/ECB/CCM
* CMAC/HMAC
* RNG 
* Generic crypto
* Data storage

## Static code analysis

Static code analysis tool used is Coverity central analysis version **2021.12.1**

## Build instructions

The FW cannot be rebuilt and is available in binary form only.




# **ELE FW NOM v0.0.5**

# Release description

This is the release note for EdgeLock Enclave (ELE) Firmware. This ELE FW is authenticated and installed by ELE ROM. It provides new features, fixes, and exposes cryptographic services to other cores through SAB.

## Existing feature changes 

* SAB API use now PSA standard
* SAB Cipher API bug fix when IV is not null
* SAB Crypto operation are now done "in place"
* SAB Key consistency is now checked in key generate API
* SAB Other minor Fix
* SAB ECC brainpool now supported

## New features

* SAB Generic crypto API
* Secure JTAG
* Commit API
* Get event

## Missing features

OEM image encryption is not yet available.

## Bug fixes

NA

# Delivery contents

## Supported revsion
* i.MX8ULP rev A0
* i.MX8ULP rev A1

## Release artifacts

The release is composed of the following artifacts:

* Sentinel documentation
[ELE Baseline API](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/Architecture/ELE%20Baseline%20API.docx?d=wdd4d3179a95746c4a21c60ecff8ad0d0&csf=1&web=1)

* SAB main documentation
[S40x Sentinel API Bridge detailed implementation](https://nxp1.sharepoint.com/:w:/r/teams/32_2/MICR_STEC/secure_boot/Shared%20Documents/S4/HSM/S40x%20Sentinel%20API%20Bridge%20detailed%20implementation.docx?d=wb05d7726aa5447b39cb14c768503ecf3&csf=1&web=1&e=fuo5MP)

## FW container location

* ELE FW container v0.0.5 can be found here 
[mx8ulpa0-ahab-container.img](https://bitbucket.sw.nxp.com/projects/MCUSEC/repos/sentinel-fw-release/browse)

## Tests description

Functional tests have been performed on the SAB features delivered:

**SAB :**
* Asymetric NIST/BRAINPOOL cypto signature/verify
* Hash 256/384/512
* SAB Key management
* Symetric crypto AES 128/256 CBC/ECB/CCM
* CMAC/HMAC
* RNG 
* Generic crypto
* Data storage

## Static code analysis

Static code analysis tool used is Coverity central analysis version **2021.12.1**

## Build instructions

The FW cannot be rebuilt and is available in binary form only.



