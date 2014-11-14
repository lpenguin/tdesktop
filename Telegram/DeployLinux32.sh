AppVersionStr=0.6.8
AppVersion=6008

if [ ! -f "./../Linux/Release/deploy/$AppVersionStr/tlinux32upd$AppVersion" ]; then
    echo "tlinux32upd$AppVersion not found!"
    exit 1
fi

if [ ! -f "./../Linux/Release/deploy/$AppVersionStr/tsetup32.$AppVersionStr.tar.xz" ]; then
    echo "tsetup32.$AppVersionStr.zip not found!"
    exit 1
fi

scp ./../Linux/Release/deploy/$AppVersionStr/tlinux32upd$AppVersion tupdates:tdesktop/static/tlinux32/
scp ./../Linux/Release/deploy/$AppVersionStr/tsetup32.$AppVersionStr.tar.xz tupdates:tdesktop/static/tlinux32/

