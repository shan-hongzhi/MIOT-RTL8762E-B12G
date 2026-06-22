..\..\prepend_header\prepend_header.exe -t app_data2 -p "SampleAppData2.bin" -m 1 -c sha256 -b 12
1>NUL ren SampleAppData2.tmp SampleImageAppData2.bin
1>NUL ren SampleAppData2_MP.bin SampleImageAppData2_MP.bin
..\..\md5\md5.exe "SampleImageAppData2_MP.bin"