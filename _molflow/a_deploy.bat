@echo off

set /p versionname="Version name (for filename):"

md Compiled\molflow_%versionname%
md Compiled\molflow_%versionname%\images
md Compiled\molflow_%versionname%\param
md Compiled\molflow_%versionname%\param\Distributions
md Compiled\molflow_%versionname%\param\Materials

copy Release\*.exe Compiled\molflow_%versionname%
copy SDL.dll Compiled\molflow_%versionname%
copy libgsl.dll Compiled\molflow_%versionname%
copy libgslcblas.dll Compiled\molflow_%versionname%
copy 7za.exe Compiled\molflow_%versionname%
copy msvcp140.dll Compiled\molflow_%versionname%
copy vcruntime140.dll Compiled\molflow_%versionname%
copy images\*.png Compiled\molflow_%versionname%\images\
copy param\Materials\*.csv Compiled\molflow_%versionname%\param\Materials
copy param\Distributions\*.csv Compiled\molflow_%versionname%\param\Distributions
copy updater_config_default.xml Compiled\molflow_%versionname%\updater_config.xml

cd Compiled
..\7za u -tzip molflow_%versionname%.zip molflow_%versionname%
cd ..

set destdir="source_snapshots\%date%_%time:~0,2%%time:~3,2% (%versionname%)"
md %destdir%
xcopy /e /exclude:snapshotexclude.txt Source_files %destdir%

set destdir="source_snapshots_shared\%date%_%time:~0,2%%time:~3,2% (molflow_%versionname%)"
md %destdir%
xcopy /e /exclude:..\_shared_sources\snapshotexclude.txt ..\_shared_sources %destdir%

curl -T Compiled\molflow_%versionname%.zip -su mszakacs https://molflow.web.cern.ch/_site/files/files/molflow_%versionname%.zip
del Compiled\molflow_%versionname%.zip