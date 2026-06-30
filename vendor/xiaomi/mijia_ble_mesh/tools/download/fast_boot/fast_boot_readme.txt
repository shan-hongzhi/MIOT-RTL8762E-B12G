一、烧录 Patch_MP_master_1.0.355.1_796d53a-fb39503470d8cc2c3c9e3ac280b6dbdd
二、烧录eFuse
方法：可以用mpcli或者MP Tool烧录附件eFuse（供电2.5V±10%）
          mpcli：版号≥1.0.4.15
          mpcli烧录eFuse需要使用“-u”选项。供电2.5V±10%后，在烧录diag Image的command        line后补上“-u [eFuse json file]”即可。
          mpcli -c comX -r -u RTL8762E_Enable_DSS-996b9509be087b6701214bf11f582527.json
          MP Tool：版号≥1.1.1.7
          在量产模式烧录烧eFuse json在附件中，是RTL8762E_Enable_DSS-996b9509be087b6701214bf11f582527.json
         Mptool的最新版本在当前目录下


如果调试使用BeeMPTool_v1.1.1.8 烧录app 分区，需要修改vendor/realtek/tools/bee/post_build.sh
./prepend_header -t app_code -p ${TARGET}.bin -m 1 -i "mp.ini" -c crc -a ../../../tool/key.json
修改成：
./prepend_header -t app_code -b 12 -p ${TARGET}.bin -m 1 -i "mp.ini" -c crc -a ../../../tool/key.json