..\..\prepend_header\prepend_header.exe -t app_data6 -p "SampleAppData6.bin" -m 1 -c sha256 -b 12
1>NUL ren SampleAppData6.tmp SampleImageAppData6.bin
1>NUL ren SampleAppData6_MP.bin SampleImageAppData6_MP.bin
..\..\md5\md5.exe "SampleImageAppData6_MP.bin"