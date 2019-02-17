--= Prerequisites =--

1) Microsoft .NET Framework 4
http://www.microsoft.com/download/en/details.aspx?id=17851

2) Windows SDK 7.1
http://www.microsoft.com/download/en/details.aspx?id=8279
untick Itanium libraries, .NET development and Common Utilities

3) Windows Driver Kit 7.1.0
http://www.microsoft.com/download/en/details.aspx?id=11800
full environment + debugging tools

4) Detours Express 3.0 Build_308
http://research.microsoft.com/en-us/downloads/d36340fb-4d3c-4ddd-bf5b-1db25d03713d/default.aspx
install it in the default directory!

5) Pin 2.10 VS10
http://www.cs.virginia.edu/kim/publicity/pin/kits/pin-2.10-45467-msvc10-ia32_intel64-windows.zip


--= Quick compile =--
1) run the compile.bat script in the windows directory
   it will produce a bin directory with all the files

--= Setup =--
1) run as adimistrator "bcdedit -debug on" and reboot

--= Compile the detour =--
1) run the sdk build environment as administrator
2) setenv /x86
3) nmake

--= Compile the driver =--
1) run the x64 Free build environment
2) bld /w

--= Test =--
1) disable driver signature enforcement (press F8 while
   booting and select the appropriate option)
2) start a cmd with as administrator
3) load the driver by executing start run.bat in the drv
   dir (ignore warning about signinig)
4) start the detoured reader by executing run_acrord.bat
   in the detour dir
