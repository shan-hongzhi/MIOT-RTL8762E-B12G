PrependHeader
======

The *prepend_header* is used to modify/add image header to binary image file, contains fill item, checksum and RSA. It can add MPHeader for  binary image file to meet the requirement of the *MPTool*.

Usage
--------------

    D:\>prpend_header --help
    	-a, --aeskey          AES Key file path
    	-b, --ictype          ic type, default:5
    	-c, --checktype       check type:one of crc, sha256
    	-e, --encryptmask     1:mask aes encrypt; 0:do encrypt, default: 0
    	-i, --mpinipath       mp.ini file path
    	-m, --mpheader        0:not add mpheader,only prepend imageheader;
    	                      1:prepend imageheader & mpheader;
    	                      2:only add mpheader,not prepend imageheader;
    	                      3:already exists mpheader, change imageheader;
    	                      default: 0
    	-o, --outkeypath      encrypted AES key file path,Out.json
    	-p, --binpath         bin file path
    	-r, --signkey         Sign Key file path
    	-s, --securitylevel   level: 0-3, default: -1
    	-t, --bintype         bin file type.
                              one of oem_cfg,factory_code,app_data1,app_data2,
                              app_data3,app_data4,app_data5,app_data6,rom_patch，                         app_code,secure_boot,ota_header,mp_rf_test,upperstack_code
                              bt_stack_patch,dsp_patch,dsp_app,mcu_cfg,dsp_data,
                              boot_patch,pmc_patch,bt_sys_patch,userdata1,userdata2
    	-u, --customerdataid    2Bytes customer data id, default:0x0000
    	-?, --help              print this message

Notes
--------------

*MPTool* requires the binary file（such as config file, OTA Header Image, Secure Boot Image, Patch Image, APP Image)  contains  MP Header.

Examples
--------------

    1. AES encryption, modify image header(checksum sha256,RSA),add MP Header.
    D:\>prepend_header -t app_code -p app.bin -m 1 -c sha256 -a key.json
    Output Image: app_MP.bin

MD5
======

The *md5.exe* is a PC tool to make the output binary image file meets the
requirement of *MPTool*.

Usage
--------------

    md5 <filename>
        Add md5 checksum to file name

Notes
--------------

MPTool requires the binary file names should be ended with -[md5sum], it will
use md5sum to check whether the binary file is valid or not.

Examples
--------------

    1. Add md5 checksum to Patch binary
    D:\>md5 Patch.bin
    Output Image: Patch-381b3b002cfa300054142e01c0332e01.bin

    2. Add md5 checksum to APP binary
    D:\>md5 app.bin
    Output Image: app-381b6f00f4fc2c0054142e01c0332e01.bin

Hex2Bin
======

The *Hex2Bin.exe* is a PC tool to make the output binary image file meets the
requirement of *MPTool*.

Usage
--------------

    Hex2Bin <input hex file> <output binary file>

Examples
--------------

    1. convert APP.hex to binary file.
    D:\>Hex2Bin.exe APP.hex app.bin
    Output Image:app.bin

signImage
======

The *signImage* is used to generate key and signimage .

Usage
--------------

    D:\>signImage --help
      -b, --ictype        ic type, default:5
      -p, --binpath       bin file path
      -r, --signkey       Sign Key file path
      -c, --checktype     check type: one of crc, sha256
      -n, --nompheader    add if there is no mp header
      -g, --genkey        genarate sign key file, key type:rsa, ecdsa, ed25519
      -i, --rsakeytype    RSA sign key len, one of rsa_3072,rsa_2048
      -?, --help          print this message
