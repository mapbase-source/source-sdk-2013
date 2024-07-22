echo CREATING SHADER EDITOR PROJECTS
pushd $0
wine ./devtools/bin/vpc.exe /2010 +shadereditor /mksln shadereditor.sln
popd
echo Done.
