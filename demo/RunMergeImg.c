/*****************************************************************************\
* RunRelabel.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2023-06-09
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"
#include "iftMetrics.h"

/* HEADERS __________________________________________________________________*/
void usage();
void readImgInputs
(iftArgs *args, iftImage **labels, iftImage **reseg, const char **path, bool *is_video);
iftImage *mergeImageORetorno
(iftImage *label_img, iftImage *reseg_img);

/* MAIN _____________________________________________________________________*/
int main(int argc, char const *argv[])
{
  /*-------------------------------------------------------------------------*/
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);
  has_req = iftExistArg(args, "labels") && iftExistArg(args, "out") && iftExistArg(args, "reseg");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  { usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
  /*-------------------------------------------------------------------------*/
  const char *OUT_PATH;
  bool is_video;
  iftImage *reseg, *label_img, *out_img;

  readImgInputs(args, &label_img, &reseg, &OUT_PATH, &is_video);
  iftDestroyArgs(&args);

  out_img = mergeImageORetorno(label_img, reseg);
  iftDestroyImage(&label_img);

  if(!is_video)
  { iftWriteImageByExt(out_img, OUT_PATH); }
  else
  { iftWriteVolumeAsSingleVideoFolder(out_img, OUT_PATH); }
  iftDestroyImage(&out_img);

  return EXIT_SUCCESS;
}

/* DEFINITIONS ______________________________________________________________*/
void usage()
{
  const int SKIP_IND = 15; // For indentation purposes
  printf("\nThe required parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--labels", 
         "Input label");
  printf("%-*s %s\n", SKIP_IND, "--reseg", 
         "reseg image image");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output relabeled colored image");



  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

void readImgInputs
(iftArgs *args, iftImage **labels, iftImage **reseg, const char **path, bool *is_video)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(args != NULL);
  // assert(img != NULL);
  assert(labels != NULL);
  assert(path != NULL);
  #endif /*##################################################################*/
  const char *PATH;

  if(iftHasArgVal(args, "labels")) 
  {
    PATH = iftGetArg(args, "labels");

    if(iftIsImageFile(PATH)) 
    { (*labels) = iftReadImageByExt(PATH); (*is_video) = false;}
    else if(iftDirExists(PATH))
    { (*labels) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); }
  }
  else iftError("No label image path was given", __func__);


  if(iftHasArgVal(args, "reseg")) 
  {
    PATH = iftGetArg(args, "reseg");

    if(iftIsImageFile(PATH)) 
    { (*reseg) = iftReadImageByExt(PATH); (*is_video) = false;}
    else if(iftDirExists(PATH))
    { (*reseg) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); }
  }
  else iftError("No label image path was given", __func__);

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);
}


iftImage *mergeImageORetorno
(iftImage *label_img, iftImage *reseg_img)
{
    int maxval;
    iftImage *newlabel_img;

    newlabel_img = iftCopyImage(label_img);
    maxval = iftMaximumValue(newlabel_img);
    printf("Maax: %d %d\n\n", iftMaximumValue(newlabel_img), iftMaximumValue(reseg_img));

    for(int p = 0; p < newlabel_img->n; ++p){
        if(reseg_img->val[p] == 2) { newlabel_img->val[p] = maxval+ 1; }
    }
    return newlabel_img;
}

// iftImage mergeImage
// (iftImage *label_img, iftImage *reseg_img)
// {
//   #ifdef IFT_DEBUG //---------------------------------------------------------|
//   assert(label_img != NULL);
//   #endif //-------------------------------------------------------------------|
//   int new_label;
//   iftSet *queue;
//   iftImage *relabel_img;
//   iftAdjRel *A;
//   iftBMap *visited;

//   relabel_img = iftCreateImage(label_img->xsize, label_img->ysize, label_img->zsize);
//   if(iftIs3DImage(label_img)) { A = iftSpheric(sqrtf(3.0)); }
//   else { A = iftCircular(sqrtf(2.0)); }

//   new_label = 0;
//   queue = NULL;
//   visited = iftCreateBMap(label_img->n);

//   for(int p = 0; p < label_img->n; ++p)
//   {
//     if(!iftBMapValue(visited, p))  
//     {
//       iftInsertSet(&queue, p); ++new_label;
//       while(queue != NULL)
//       {
//         int x;
//         iftVoxel x_vxl;

//         x = iftRemoveSet(&queue);
//         x_vxl = iftGetVoxelCoord(label_img, x);
//         iftBMapSet1(visited, x);
//         relabel_img->val[x] = new_label;

//         for(int i = 1; i < A->n; ++i)
//         {
//           iftVoxel y_vxl;

//           y_vxl = iftGetAdjacentVoxel(A, x_vxl, i);  

//           if(iftValidVoxel(label_img, y_vxl))
//           {
//             int y;

//             y = iftGetVoxelIndex(label_img, y_vxl);

//             if(label_img->val[x] == label_img->val[y] && !iftBMapValue(visited, y))
//             { iftInsertSet(&queue, y); }
//           }
//         }
//       }
//     }
//   }

//   iftDestroySet(&queue);
//   iftDestroyBMap(&visited);
//   iftDestroyAdjRel(&A);

//   return relabel_img;
// }