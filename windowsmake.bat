set project=windows\NetworkHelper\NetworkHelper.sln
set target=
set configuration=/property:Configuration=Release

IF "%1"=="clean" set target=/target:Clean

MSBuild %configuration% %target% %project%
