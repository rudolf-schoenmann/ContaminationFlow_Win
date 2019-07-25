set destdir="source_snapshots_shared\%date%_%time:~0,2%%time:~3,2%"
md %destdir%
robocopy ..\_shared_sources %destdir% /e /xd .git