
cd ..
git apply --check --verbose i330R_dsx/0001-Added-i330R-support.patch
pwd
cd libdivecomputer/
git apply --check --verbose ../i330R_dsx/0001-Reenabled-i330R-and-DSX.patch 
cd ../..
