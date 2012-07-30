cd $TEMP_DIR
mkdir -p $MTPOINT

$MOUNT_ORI_EXE -o repo=$SOURCE_REPO $MTPOINT
sleep 3

echo "hello" > $MTPOINT/hello
echo "file 2" > $MTPOINT/file2.tst
mv $MTPOINT/file2.tst $MTPOINT/file3.tst
cp $MTPOINT/file10.tst $MTPOINT/file11.tst
echo "testing" > $MTPOINT/file3.tst
mv $MTPOINT/file11.tst $MTPOINT/file3.tst

echo "commit" > $MTPOINT/.ori_control
sleep 3

cd $SOURCE_REPO
$ORI_EXE checkout

$PYTHON $SCRIPTS/compare.py "$SOURCE_REPO" "$MTPOINT"

umount $MTPOINT

