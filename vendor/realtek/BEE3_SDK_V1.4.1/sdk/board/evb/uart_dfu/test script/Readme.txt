1. Information such as baud rate can be modified in setting_data.py. The details are as follows.
    'BOUNRATE':                            The used band rate. The default value is 115200.
    'COM':                                       Serial number.
    'PATH':                                      Directory and file name of the file to be upgraded.
    'IC_TYPE':                                   Type number of the used chip. bee3:0x0c; sbee2:0x09; bee2:0x05
    'SUPPORT_VERSION_CHECK':  Whether version checking is supported. The value can be True or False.
    'TEST_TIMES':                            Number of consecutive tests. The minimum is 1.
    'READ_INFORMATION_ONLY' Whether to only read the information without upgrading the version. The value can be True or False.

2. Run command
    python uart_dfu.py