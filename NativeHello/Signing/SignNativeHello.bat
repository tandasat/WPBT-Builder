::
:: Signs NativeHello.exe in the same directory using the accompanying .pfx and
:: .cer files. The most parts of this batch and the way to produce the functional
:: .pfx file (below) was shared by Mickey (@HackingThings).
::
:: HT_Srl.pfx is a PKCS12 file containing a private key. This is re-created from
:: the leaked and revoked PKCS12 file found here:
:: https://github.com/hackedteam/rcs-db/blob/6cff59d28634d718cac9fdd17cb629fd59a3cf3f/nsis/HT.pfx
:: then, it is imported (with leaked password "GeoMornellaChallenge7") and
:: exported with DigiCert Certificate Utility.
:: https://www.digicert.com/kb/util/csr-creation-microsoft-servers-using-digicert-utility.htm
::
:: "VeriSignG5.cer" is a X.509 certificate that is taken from here:
:: https://github.com/kxproject/kx-audio-driver/blob/master/VeriSignG5.cer
::
:: Figuring out why they work is left as an exercise for readers.
::
@echo off

net session >nul 2>&1
if %errorLevel% NEQ 0 (
    echo ERROR: Administrator privileges is required.
    exit /b 1
)

echo Changing the system clock for signing.
date 04-04-15

"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64\signtool.exe" sign /v /ac "VeriSignG5.cer" /f HT_Srl.pfx /p 1234 /fd sha1 /nph /debug NativeHello.exe
if %errorLevel% NEQ 0 (
    echo ERROR: signtool failed.
)

echo Restoring the system clock for signing.
net stop w32time >NUL
net start w32time >NUL
w32tm /resync /nowait >NUL
