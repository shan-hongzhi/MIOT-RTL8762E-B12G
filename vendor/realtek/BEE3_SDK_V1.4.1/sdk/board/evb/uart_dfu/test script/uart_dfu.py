# -*- coding: utf-8 -*-

import serial
import serial.tools.list_ports
from time import sleep
import sys
from setting_data import setting_table

class DFU():
    def __init__(self):
        self.__dfu_get_setting_data()
        self.__dfu_deinit_data()
        self.__dfu_init_table()
        self.__open_serial()

    def __dfu_get_setting_data(self):
        self.__boudrate = setting_table['BOUDRATE']
        self.__com = setting_table['COM']
        self.__path = setting_table['PATH']
        #__ic_type => bee3:0x0c; sbee2:0x09; bee2:0x05
        self.__ic_type = setting_table['IC_TYPE'] 
        self.__version_check = setting_table['SUPPORT_VERSION_CHECK']
        if setting_table['TEST_TIMES'] > 0:
            self.__test_times = setting_table['TEST_TIMES']
        else:
            self.__test_times = 1
        self.__read_information_only = setting_table['READ_INFORMATION_ONLY']
        print('test_times =', self.__test_times)

    def __dfu_deinit_data(self):
        self.__payload_len = 0
        self.__is_dfu_support_multi_img = 0
        self.__is_dfu_support_bank_switch = 0
        self.__active_bank = 0
        self.__file_total_size = 0
        self.__file_bank0_size = 0
        self.__temp_bank_size = 0
        self.__origin_img_indicator = 0
        self.__is_single_file_update = 0
        self.__sub_image_num = 0
        self.__sub_image_address = []
        self.__sub_image_size = []                           
        self.__sub_image_id_index = [0 for col in range(14)]
        self.__origin_version = {}       

    def __dfu_init_table(self):
        self.__cmd_dict = {
            'START_DFU':[0x03,0x01,0x12,0x0C,0x00],
            'DFU_WRITE_IMAGE':[0x03,0x02,0x12,self.__payload_len&0xff,self.__payload_len>>8],
            'DFU_VALID_IMAGE':[0x03,0x03,0x12,0x00,0x00],
            'DFU_ACTIVE_RESET':[0x03,0x04,0x12,0x00,0x00],
            'DFU_SYS_RESET':[0x03,0x05,0x12,0x00,0x00],
            'READ_DEVICE_INFO':[0x03,0x06,0x12,0x00,0x00],
            'READ_VERSION_INFO':[0x03,0x07,0x12,0x00,0x00]}
        self.__response_dict = {
            'START_DFU':[0x04,0x01,0x12,0x00,0x01,0x00,0x00],
            'DFU_WRITE_IMAGE':[0x04,0x02,0x12,0x00,0x05,0x00],
            'DFU_VALID_IMAGE':[0x04,0x03,0x12,0x00,0x01,0x00,0x00],
            'DFU_ACTIVE_RESET':[0x04,0x04,0x12,0x00,0x01,0x00],
            'DFU_SYS_RESET':[0x04,0x05,0x12,0x00,0x01,0x00],
            'READ_DEVICE_INFO':[0x04,0x06,0x12,0x00,0x0c,0x00,self.__ic_type],
            'READ_VERSION_INFO':[0x04,0x07,0x12,0x00]}
        self.__img_id_list = [
            ['SOVC Config', 0x0], ['System Config', 0x278F],
            ['OTA Header', 0x2790], ['Secure Boot', 0x2791],
            ['ROM Patch', 0x2792], ['APP Image', 0x2793],
            ['APP Data1', 0x2794], ['APP Data2', 0x2795],
            ['APP Data3', 0x2796], ['APP Data4', 0x2797],
            ['APP Data5', 0x2798], ['APP Data6', 0x2799],                                                                                                                                                                      
            ['Upper Stack', 0x279a], ['Reserve1', 0x0],
            ['Reserve2', 0x0],['Reserve3', 0x0]]
        self.dual_bank_required_file_index_list = [2,3,4,5,12]#ota header/fsbl/patch/app image/upperstack image

    def __open_serial(self):
        self.__ser = serial.Serial(self.__com ,self.__boudrate,timeout=3)
        if self.__ser.isOpen():
            print(self.__com, "open success")
        else :
            print(self.__com, "open failed")

    def __dfu_get_crc16_value(self,x):
        a = 0xFFFF
        b = 0xA001
        for byte in x:
            a ^= byte
            for i in range(8):
                last = a % 2
                a >>= 1
                if last == 1:
                    a ^= b
        return [a&0xff,a>>8]

    def __dfu_check_crc(self,payload,crc_value):
        if self.__dfu_get_crc16_value(payload) == crc_value:
            return True
        else:
            return False

    def __dfu_get_4_bytes_value(self, index, byte = []):
        if len(byte) < index + 4:
            print('[dfu_get_4_bytes_value]input is invalid')
            sys.exit(1)
        else:
            result = byte[index] | (byte[index+1] << 8) | (byte[index+2] << 16) | (byte[index+3] << 24)
            #print('[dfu_get_4_bytes_value] result = ', hex(result))
            return result

    def __dfu_get_version_num(self, signature, index, byte = []):
        version = []
        if signature == 0x2790:
            version.insert(0,byte[index+3]&0xf)
            version.insert(2,byte[index+2]&0xf)
            version.insert(3,byte[index+1]&0xf)
            version.insert(4,byte[index]&0xf)
        else:
            version.insert(0,byte[index]&0xf)
            version.insert(1,(byte[index]>>4)|((byte[index+1]&0xf)<<4))
            version.insert(2,(byte[index+1]>>4)|((byte[index+2]&0xf)<<4)|(byte[index+2]>>4)|((byte[index+3]&0x7)<<4))
            version.insert(3,(byte[index+3]&0xf8)>>3)
        return version

    def __dfu_check_sub_image_type(self, index):
        if (index < 2) or (index >29) or ((index > 13) and (index < 18)):
            return False
        return True

    def __dfu_check_packet(self, f):
        #get image size          
        f.seek(0x02)
        tmp = f.read(4)
        self.__file_total_size = self.__dfu_get_4_bytes_value(0, tmp)
        print('total image size = %#x' % self.__file_total_size)
        
        #check ic type
        f.seek(0x27)
        packet_ic_type = f.read(1)
        print('ic type = %#x' % packet_ic_type[0])
        if packet_ic_type[0] != self.__ic_type:
            print('wrong ic_type')
            return False
        
        #get sub image type              				
        f.seek(0x28)
        tmp = f.read(4)
        sub_file_indicator = self.__dfu_get_4_bytes_value(0, tmp)
        print('sub file indicator = %#x' % sub_file_indicator)
        self.__sub_image_num = 0
        for i in range(32):
            tmp = 0x0001 << i
            if (sub_file_indicator & tmp):
                self.__sub_image_id_index[self.__sub_image_num] = i
                self.__sub_image_num += 1
                if self.__dfu_check_sub_image_type(i) == False:
                    print(self.__img_id_list[i % 16][0])
                    print('sub imge type is invalid')
                    return False                               
        print('sub image num = %d' % self.__sub_image_num) 
        return True

    def __dfu_read_device_info(self):    
        rlen = self.__ser.inWaiting()
        data = self.__ser.read(rlen)
        print(" " .join(hex(n) for n in list(data)))
        if list(data[0:7]) == self.__response_dict['READ_DEVICE_INFO'] \
                        and self.__dfu_check_crc(list(data[0:len(data)-2]),list(data[len(data)-2:len(data)])):
            self.__payload_len = data[10]|data[11]<<8
            self.__is_dfu_support_multi_img = (data[9] & 0x10) >> 4
            self.__temp_bank_size = data[12] << 12
            self.__origin_img_indicator = self.__dfu_get_4_bytes_value(14, data)
            tmp = (self.__origin_img_indicator >> 4) & 0x3
            if tmp == 1 or tmp == 2:
                self.__active_bank = tmp - 1
                self.__is_dfu_support_bank_switch = 1

            print('is_dfu_support_bank_switch = ', self.__is_dfu_support_bank_switch, 'active_bank = ', self.__active_bank)                              
            print('temp_bank_size = %dk' % (self.__temp_bank_size >> 10), '(%#x)' % self.__temp_bank_size, 'is_dfu_support_multi_img = ', self.__is_dfu_support_multi_img)
            print('payload_len = ', self.__payload_len, 'origin_img_indicator = %#x' % self.__origin_img_indicator)

            if self.__is_dfu_support_bank_switch == 1:
                if  self.__is_single_file_update== 1:
                    print('bank switch does not support unpacked single file updating')
                    return False
                elif self.__is_dfu_support_multi_img != 1:
                    print('bank switch does not support non-multi image updating')
                    return False                    
        else:
            print('dfu_read_device_info, frame_error_code =', data[3])
            return False
        return True

    def __dfu_check_package_files(self):
        if self.__is_dfu_support_bank_switch:
            offset = 0
            if self.__active_bank == 0:
                offset = 16
            
            j = 0
            for i in range(self.__sub_image_num):
                #print(self.__sub_image_id_index[i])
                if(self.__sub_image_id_index[i] == (self.dual_bank_required_file_index_list[j] + offset)):
                    j = j + 1
                    if(j == len(self.dual_bank_required_file_index_list)):
                        return True
                        
            return False
            
    def __dfu_read_version_info(self):
        rlen = self.__ser.inWaiting()
        data = self.__ser.read(rlen)
        print(" " .join(hex(n) for n in list(data)))
        if list(data[0:4]) == self.__response_dict['READ_VERSION_INFO'] \
                        and self.__dfu_check_crc(list(data[0:len(data)-2]),list(data[len(data)-2:len(data)])):
            
            tmp_indicator = self.__origin_img_indicator
            tmp_image_exist_num = 0
            for i in range(0,len(self.__img_id_list)):
                if tmp_indicator & 0x3:
                    self.__origin_version[self.__img_id_list[i][1]] = self.__dfu_get_version_num(self.__img_id_list[i][1], 6+tmp_image_exist_num*4,data)
                    tmp_image_exist_num += 1
                    print('origin %s version:' % self.__img_id_list[i][0], self.__origin_version[self.__img_id_list[i][1]])
                tmp_indicator >>= 2    
            print('image exist num', tmp_image_exist_num)
            if (data[4] | data[5] << 8) != (tmp_image_exist_num * 4):
                print('payload len is wrong')
                return False
        else:
            print('dfu_read_version_info, frame_error_code =', data[3])
            return False
        return True    

    def __dfu_check_version(self,signature,file_version):
        result = False 

        for i in range(4):
            if self.__origin_version[signature][i] < file_version[i]:
                result = True

        # if the app data file doesn't exist, the version is 15.255.255.31
        if result == False:
            if signature >= 0x2794 and signature <= 0x2799:
                tmp_version = [15,255,255,31]
                result_flag = True           
                for i in range(4):
                    if self.__origin_version[signature][i] != tmp_version[i]:
                        result_flag = False
                        break
                if result_flag == True:
                    print('app data file does not exist')
                    result = True

        print('file version num =', file_version)
        print('dfu_check_version result =', result)
        return result

    def __dfu_check_write_image_response(self,pay_load,length,retry_cnt):
        sleep(0.5)
        rlen = self.__ser.inWaiting()
        data = self.__ser.read(rlen)
        print(" " .join(hex(n) for n in list(data)))
        if list(data[0:6]) != self.__response_dict['DFU_WRITE_IMAGE'] or \
                        (False == self.__dfu_check_crc(list(data[0:len(data)-2]),list(data[len(data)-2:len(data)]))):
            print('DFU_WRITE_IMAGE fail, frame_error_code =', data[3])
            sys.exit(1)
        if data[10] == 0:
            return True
        else:
            print('DFU_WRITE_IMAGE retry, payload_error_code = ', data[10])
            if retry_cnt < 5:
                self.__cmd_dict['DFU_WRITE_IMAGE'][3] = length&0xff
                self.__cmd_dict['DFU_WRITE_IMAGE'][4] = length>>8
                next_send_offset = self.__dfu_get_4_bytes_value(6, data)
                self.__ser.write(self.__cmd_dict['DFU_WRITE_IMAGE']+pay_load[next_send_offset:next_send_offset+length]+ \
                        self.__dfu_get_crc16_value(self.__cmd_dict['DFU_WRITE_IMAGE']+pay_load[next_send_offset:next_send_offset+length]))
            else:
                print('DFU_WRITE_IMAGE retry over 5 times')
                sys.exit(1)
            return False

    def __dfu_handle_valid_image(self):
        sleep(0.5)
        rlen = self.__ser.inWaiting()
        data = self.__ser.read(rlen)
        if rlen:
            print(" " .join(hex(n) for n in list(data)))
            if list(data[0:7]) != self.__response_dict['DFU_VALID_IMAGE'] \
                or (False == self.__dfu_check_crc(list(data[0:len(data)-2]),list(data[len(data)-2:len(data)]))):
                print('DFU_VALID_IMAGE fail, frame_error_code =', data[3], 'payload_error_code = ', data[6])
                sys.exit(1)
            return True
        else:
            return False  

    def __dfu_handle_stess_test(self, time):
        if time <= self.__test_times:
            self.__dfu_deinit_data()
            sleep(5)

    def run(self):
        try:
            with open (self.__path,'rb') as f:
                test_count = 1                
                while test_count <= self.__test_times:
                    print('\n[*** TEST TIMES %d ***]\n' % test_count)
                    test_count += 1

                    #check packet
                    f.seek(0x0)
                    tmp = f.read(2)
                    packet_signature = tmp[0] | (tmp[1] << 8)
                    if packet_signature == 0x4D47:
                        print('packet image without user data')
                        if self.__dfu_check_packet(f) == False:
                            sys.exit(1)
                    else:
                        print('not packet image without user data')

                        #check whether the file is the unpacked single file
                        f.seek(0x200)
                        tmp_ic_type = f.read(1)
                        f.seek(0x204)
                        tmp = f.read(2)     
                        tmp_signature = tmp[0] | (tmp[1] << 8)                       
                        if (tmp_ic_type[0] == self.__ic_type) and (tmp_signature >= 0x2790) and (tmp_signature <= 0x279a):
                            self.__is_single_file_update = 1
                            self.__sub_image_num = 1
                            for i in range(16):
                                if self.__img_id_list[i][1] == tmp_signature:
                                    self.__sub_image_id_index[0] = i
                                    break
                            print('unpacked single file [%s] updates.' % self.__img_id_list[i][0], 'signature = %#x' % tmp_signature)
                        else:
                            print('file does not support update now')
                            sys.exit(1)                        

                    #start dfu          
                    print('\n【***step 1***】 read device information,opCode = 0x1206')
                    self.__ser.write(0x01)#in order to exit dlps
                    sleep(0.5)
                    self.__ser.write(self.__cmd_dict['READ_DEVICE_INFO']+self.__dfu_get_crc16_value(self.__cmd_dict['READ_DEVICE_INFO']))
                    sleep(0.5)
                    if self.__dfu_read_device_info() == False:
                        print('READ_DEVICE_INFO fail')
                        sys.exit(1)                     

                    print('\n【***step 2***】 get sub image address and size')
                    if self.__is_single_file_update == 1:
                        f.seek(0x208)
                        tmp = f.read(4)                        
                        self.__sub_image_size.insert(0, (self.__dfu_get_4_bytes_value(0, tmp) + 0x200 + 0x400)) 
                        print('size = %#x' % self.__sub_image_size[0])                                               
                    else:  
                        if self.__read_information_only == False:
                            #if support dual bank, check whether the files in package is complete and correct
                            if self.__dfu_check_package_files() == False:
                                print('package files error')
                                sys.exit(1)  
                        else:
                            print('read information only. Do not check package files')      
                                                 
                        self.__file_bank0_size = 0
                        bank0_image_num = 0
                        for i in range(self.__sub_image_num):
                            f.seek(0x2c + 0xc * i)
                            tmp = f.read(4)
                            self.__sub_image_address.insert(i, self.__dfu_get_4_bytes_value(0, tmp))

                            f.seek(0x30 + 0xc * i)
                            tmp = f.read(4)                        
                            self.__sub_image_size.insert(i, self.__dfu_get_4_bytes_value(0, tmp))
                            print(self.__img_id_list[self.__sub_image_id_index[i] % 16][0], \
                                'address = %#x' % self.__sub_image_address[i], 'size = %#x' % self.__sub_image_size[i])
                            if self.__is_dfu_support_bank_switch == 1 and self.__sub_image_id_index[i] < 16:
                                self.__file_bank0_size +=  self.__sub_image_size[i]
                                bank0_image_num += 1  
                        print('bank0_image_num = ', bank0_image_num);

                    print('\n【***step 3***】 cal the start address and start file num')
                    start_file_num = 0
                    if self.__is_single_file_update == 1:
                        sub_image_offset = 0
                    else:
                        sub_image_offset = 0x2c + 0xc * self.__sub_image_num
                        if self.__is_dfu_support_bank_switch == 1:
                            if self.__active_bank == 0:
                                sub_image_offset += self.__file_bank0_size
                                if bank0_image_num != 0:
                                    start_file_num = self.__sub_image_num - bank0_image_num
                            elif self.__active_bank == 1:
                                self.__sub_image_num = bank0_image_num
                    print('sub img num = ', self.__sub_image_num,'start file num =',(start_file_num + 1))

                    print('\n【***step 4***】 read version information,opCode = 0x1207')
                    self.__ser.write(self.__cmd_dict['READ_VERSION_INFO']+self.__dfu_get_crc16_value(self.__cmd_dict['READ_VERSION_INFO']))
                    sleep(0.5)
                    if self.__dfu_read_version_info() == False:
                        print('READ_VERSION_INFO fail')
                        sys.exit(1)
                    if self.__read_information_only == True:
                        print('read information done. EXIT')
                        sys.exit(1)

                    # check whether the size of the files to be updated is over the bank size in dual bank mode
                    if self.__is_dfu_support_bank_switch == 1:
                        sum_file_size = 0
                        for tmp_index in range(start_file_num, self.__sub_image_num):
                            sum_file_size += (self.__sub_image_size[tmp_index] - 0x200)
                        if sum_file_size > self.__temp_bank_size:
                            print('The size of the files to be updated is over the bank size.')
                            sys.exit(1)  

                    # check sub image info first
                    tmp_sub_image_offset = sub_image_offset
                    sub_image_check_result = []
                    for tmp_sub_image_num in range(start_file_num, self.__sub_image_num): 
                        # init value 0
                        sub_image_check_result.insert(tmp_sub_image_num - start_file_num, 0)

                        # OTA header can not be updated in non dual bank mode
                        if self.__is_dfu_support_bank_switch == 0:
                            if self.__img_id_list[self.__sub_image_id_index[tmp_sub_image_num] % 16][1] == 0x2790:
                                sub_image_check_result[tmp_sub_image_num - start_file_num] = 1 # sub file is OTA header
                                continue
                        
                         # check whether one file size if over bank size
                        if (self.__sub_image_size[tmp_sub_image_num] - 0x200) > self.__temp_bank_size:
                            sub_image_check_result[tmp_sub_image_num - start_file_num] = 2 # sub file is over bank size 
                            continue                     

                        f.seek(tmp_sub_image_offset + 0x204)
                        signature = f.read(2)
                        signature = signature[0] + (signature[1] << 8)
                        if signature == 0x2790:
                            f.seek(tmp_sub_image_offset + 0x394)
                        else:
                            f.seek(tmp_sub_image_offset + 0x260)
                        git_version = f.read(4)
                        if ((signature >= 0x2790) and (signature <= 0x279a)):
                            # check version
                            if False == self.__dfu_check_version(signature, self.__dfu_get_version_num(signature, 0, git_version)):
                                if self.__version_check:
                                    if ((self.__is_dfu_support_bank_switch == 1) and (signature == 0x2790)) \
                                        or (self.__is_single_file_update == 1):
                                        sub_image_check_result[tmp_sub_image_num - start_file_num] = 3 # sub file version check fail
                                    elif (self.__is_dfu_support_bank_switch == 0):
                                        sub_image_check_result[tmp_sub_image_num - start_file_num] = 4 # sub file version check fail                                                                           
                        
                        tmp_sub_image_offset += self.__sub_image_size[tmp_sub_image_num]

                    current_file_size = 0
                    for tmp_sub_image_num in range(start_file_num, self.__sub_image_num):
                        print('********************************************************')
                        print('\n【current file %d: %s】' % (1 + tmp_sub_image_num - start_file_num, self.__img_id_list[self.__sub_image_id_index[tmp_sub_image_num] % 16][0]), \
                            '【total file num %d】' % (self.__sub_image_num - start_file_num), \
                            'sub_image_offset = %#x' % sub_image_offset)
                        print('\n【***File %d / step 5***】 check file size and version' % (1 + tmp_sub_image_num - start_file_num))

                        if sub_image_check_result[tmp_sub_image_num - start_file_num] == 1:
                            # OTA header can not be updated in non dual bank mode
                            sub_image_offset +=  self.__sub_image_size[tmp_sub_image_num]
                            print('OTA header can not be updated in non dual bank mode')                               
                            continue 
                        elif sub_image_check_result[tmp_sub_image_num - start_file_num] == 2: 
                            # check whether one file size if over bank size
                            sub_image_offset +=  self.__sub_image_size[tmp_sub_image_num]
                            print('current file size is over bank size')                            
                            continue                            
                        elif sub_image_check_result[tmp_sub_image_num - start_file_num] == 3:
                            print('version check fail! Do not need to update in dual bank mode or single file updating mode')
                            break
                        elif sub_image_check_result[tmp_sub_image_num - start_file_num] == 4:
                            sub_image_offset +=  self.__sub_image_size[tmp_sub_image_num]
                            print('version check fail! Do not need to update in non dual bank mode')
                            continue
                        else:
                            print('current file supports updating')

                        print('\n【***File %d / step 6***】 start dfu,opCode = 0x1201' % (1 + tmp_sub_image_num - start_file_num))
                        f.seek(sub_image_offset + 0x200)
                        Image_header = f.read(12)
                        send_list = self.__cmd_dict['START_DFU']+list(Image_header[0:12])
                        send_list = send_list+self.__dfu_get_crc16_value(send_list)              
                        self.__ser.write(send_list)
                        sleep(0.5)
                        rlen = self.__ser.inWaiting()
                        data = self.__ser.read(rlen)
                        print(" " .join(hex(n) for n in list(data)))
                        if list(data[0:7]) != self.__response_dict['START_DFU'] \
                                or (False == self.__dfu_check_crc(list(data[0:len(data)-2]),list(data[len(data)-2:len(data)]))):
                            print('START_DFU fail, frame_error_code =', data[3], 'payload_error_code = ', data[6])
                            sys.exit(1)
                        
                        print('\n【***File %d / step 7***】 write images,opCode = 0x1202' % (1 + tmp_sub_image_num - start_file_num))
                        f.seek(sub_image_offset + 0x200)
                        pay_load = list(f.read(self.__sub_image_size[tmp_sub_image_num] - 0x200))
                        self.__cmd_dict['DFU_WRITE_IMAGE'][3] = self.__payload_len&0xff
                        self.__cmd_dict['DFU_WRITE_IMAGE'][4] = self.__payload_len>>8
                        
                        for i in range(len(pay_load)//self.__payload_len):
                            self.__ser.write(self.__cmd_dict['DFU_WRITE_IMAGE']+pay_load[i*self.__payload_len:(i+1)*self.__payload_len]+
                                        self.__dfu_get_crc16_value(self.__cmd_dict['DFU_WRITE_IMAGE']+pay_load[i*self.__payload_len:(i+1)*self.__payload_len]))
                            retry_count = 0
                            while True:
                                retry_count += 1
                                if self.__dfu_check_write_image_response(pay_load,self.__payload_len,retry_count):
                                    break
                        self.__cmd_dict['DFU_WRITE_IMAGE'][3] = (len(pay_load)%self.__payload_len)&0xff
                        self.__cmd_dict['DFU_WRITE_IMAGE'][4] = (len(pay_load)%self.__payload_len)>>8
                        self.__ser.write(self.__cmd_dict['DFU_WRITE_IMAGE']+pay_load[(len(pay_load)//self.__payload_len)*self.__payload_len:len(pay_load)]+
                                        self.__dfu_get_crc16_value(self.__cmd_dict['DFU_WRITE_IMAGE']+pay_load[(len(pay_load)//self.__payload_len)*self.__payload_len:len(pay_load)]))
                        retry_count = 0
                        while True:
                            retry_count += 1
                            if self.__dfu_check_write_image_response(pay_load,len(pay_load)%self.__payload_len,retry_count):
                                break                         

                        print('\n【***File %d / step 8***】 valid image,opCode = 0x1203' % (1 + tmp_sub_image_num - start_file_num)) 
                        self.__ser.write(self.__cmd_dict['DFU_VALID_IMAGE']+self.__dfu_get_crc16_value(self.__cmd_dict['DFU_VALID_IMAGE']))
                        retry_count = 0
                        while True:
                            retry_count += 1
                            if self.__dfu_handle_valid_image():
                                break
                            elif retry_count >= 10:
                                print('DFU_VALID_IMAGE retry over 10 times')
                                sys.exit(1)  

                        # check whether 0x1204 should be sent when supporting multi img
                        is_need_to_send_active_reset = 0
                        if (self.__is_dfu_support_bank_switch == 0) and (self.__is_dfu_support_multi_img == 1):
                            current_file_size += (self.__sub_image_size[tmp_sub_image_num] - 0x200)
                            index = tmp_sub_image_num
                            while (self.__sub_image_num - 1) > index:
                                index += 1
                                if sub_image_check_result[index - start_file_num] == 0:
                                    print('current + next file size: %#x' % (current_file_size + self.__sub_image_size[index] - 0x200))
                                    if (current_file_size + self.__sub_image_size[index] - 0x200) > self.__temp_bank_size:
                                        is_need_to_send_active_reset = 1 
                                    break
                                elif (self.__sub_image_num - 1) == index:
                                    print('The last file in the package to be updated')
                                    is_need_to_send_active_reset = 1
                            print('current_file_size: %#x' % current_file_size, 'is_need_to_send_active_reset: %d' % is_need_to_send_active_reset)

                        # send opCode 0x1204
                        if (self.__is_dfu_support_multi_img == 0) or ((tmp_sub_image_num + 1) == self.__sub_image_num) \
                            or (self.__is_single_file_update == 1) or (is_need_to_send_active_reset == 1):
                            print('\n【***File %d / step 9***】 active reset,opCode = 0x1204' % (1 + tmp_sub_image_num - start_file_num))
                            current_file_size = 0 
                            self.__ser.write(self.__cmd_dict['DFU_ACTIVE_RESET']+self.__dfu_get_crc16_value(self.__cmd_dict['DFU_ACTIVE_RESET']))
                            sleep(0.5)
                            rlen = self.__ser.inWaiting()
                            data = self.__ser.read(rlen)
                            print(" " .join(hex(n) for n in list(data))) 
                            if list(data[0:6]) == self.__response_dict['DFU_ACTIVE_RESET'] \
                                and self.__dfu_check_crc(list(data[0:len(data)-2]),list(data[len(data)-2:len(data)])):                                                      
                                if data[6] == 0:
                                    print('DFU_ACTIVE_RESET success')
                                    if (tmp_sub_image_num + 1) != self.__sub_image_num:
                                        sleep(5)#wait for reboot
                                        self.__ser.write(0x01)#in order to exit dlps
                                        sleep(1)
                                else:
                                    print('DFU_ACTIVE_RESET fail')
                            else:
                                print('DFU_ACTIVE_RESET fail, frame_error_code =', data[3])

                        sub_image_offset += self.__sub_image_size[tmp_sub_image_num]  
                    
                    self.__dfu_handle_stess_test(test_count)

        finally:
            self.__ser.close()

dfu = DFU()
dfu.run()
