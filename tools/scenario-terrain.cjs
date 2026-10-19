// Only call after explicitly clearing objects and flattening authored terrain.
// Initial owner (byte 1), resources (byte 3) and other flags stay intact.
const CLEARED_FLAGS=0x43;
// Remove the object of one cell and the flags tied to it; landscape, owner and resources stay.
function clearObjectCell(objects,at) {
  objects[at]=0;
  objects[at+2]&=~CLEARED_FLAGS;
}
function prepareGrassCell(land,objects,at,dark) {
  land[at]=10;land[at+1]=28;land[at+2]=dark?88:24;
  clearObjectCell(objects,at);
}
module.exports={prepareGrassCell,clearObjectCell,CLEARED_FLAGS};
