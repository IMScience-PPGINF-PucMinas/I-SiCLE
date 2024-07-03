/*****************************************************************************\
* RunRelabel.c
*
* AUTHOR  : Lucca Lacerda
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
(iftArgs *args, iftImage **img, iftImage **labels, iftCSV **csv, const char **path, 
  bool *is_video);
iftImage *resegIFT
(iftImage *orig_img, iftImage *mask_img, long *X_value, long *Y_value);
void getCSVCoord(const iftCSV *csv, const char *label, long *X_value, long *Y_value);


/* MAIN _____________________________________________________________________*/
int main(int argc, char const *argv[])
{
  /*-------------------------------------------------------------------------*/
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);
  has_req = iftExistArg(args, "labels") && iftExistArg(args, "out") && iftExistArg(args, "reseg") && iftExistArg(args,"file");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  { usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
  /*-------------------------------------------------------------------------*/
  const char *OUT_PATH;
  bool is_video;
  iftImage *reseg, *label_img, *out_img;
  iftCSV *csv;
  long X_value[2];
  long Y_value[2];

  printf("criou\n");
  readImgInputs(args, &reseg, &label_img, &csv, &OUT_PATH, &is_video);
  
  getCSVCoord(csv, "Seed", X_value, Y_value); 

  for(int i = 0; i<2;i++)
  {
    printf("X = %ld Y = %ld\n",X_value[i],Y_value[i]);
  }

  iftDestroyArgs(&args);

  out_img = resegIFT(label_img, reseg, X_value, Y_value);
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
    printf("%-*s %s\n", SKIP_IND, "--file", 
         "File with the seeds coordenates information ");



  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

void readImgInputs
(iftArgs *args, iftImage **img, iftImage **labels, iftCSV **csv, const char **path, 
  bool *is_video)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(args != NULL);
  assert(labels != NULL);
  assert(path != NULL);
  #endif //------------------------------------------------------------------//
  const char *PATH;

  if(iftHasArgVal(args, "labels") == true) 
  {
    PATH = iftGetArg(args, "labels");

    if(iftIsImageFile(PATH) == true) 
    { (*labels) = iftReadImageByExt(PATH); (*is_video) = false; }
    else if(iftDirExists(PATH) == true)
    { (*labels) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); } 
  }
  else iftError("No label image path was given", __func__);

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);

  if(iftExistArg(args, "reseg") == true)
  {
    if(iftHasArgVal(args, "reseg") == true) 
    {
      PATH = iftGetArg(args, "reseg");

      if(iftIsImageFile(PATH) == true) 
      { (*img) = iftReadImageByExt(PATH); (*is_video) = false; }
      else if(iftDirExists(PATH) == true)
      { (*img) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
      else { iftError("Unknown image/video format", __func__); } 
    }
    else iftError("No original image path was given", __func__);
  }
  else (*img) = NULL;
  if(iftExistArg(args,"file") == true){
    if(iftHasArgVal(args,"file") == true){
		const char *VAL = iftGetArg(args,"file");
		*csv = iftReadCSV(VAL,';');

    }
  }
}

iftImage *resegIFT
(iftImage *orig_img, iftImage *mask_img, long *X_value, long *Y_value)
{
  double *dist_map;
  int *root_map;
  iftAdjRel *A;
  iftDHeap *heap;
  iftImage *label_img;
  iftMImage *mimg;

  label_img = iftCreateImage(orig_img->xsize, orig_img->ysize, orig_img->zsize);
  dist_map = calloc(label_img->n, sizeof(double));
  root_map = calloc(label_img->n, sizeof(int));
      printf("comecou %d\n", orig_img->Cb == NULL);
  mimg = iftImageToMImage(orig_img, LAB_CSPACE);
      printf("comecou\n");

  if(iftIs3DImage(label_img) == true) 
  { A = iftSpheric(sqrtf(3.0)); }
  else { A = iftCircular(sqrtf(2.0)); }

  heap = iftCreateDHeap(label_img->n, dist_map);
  iftSetRemovalPolicyDHeap(heap, MINVALUE);
      printf("1\n");

  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for
  #endif //-------------------------------------------------------------------|
  for(int v_index = 0; v_index < label_img->n; ++v_index)
  {
    root_map[v_index] = -1;
    if(mask_img->val[v_index] == 0) // if(old_label->val[v_index] == marked_superpixel)
    {
      label_img->val[v_index] = 0; // Rótulo de fundo 
      dist_map[v_index] = IFT_INFINITY_DBL_NEG;
    }
    else {
      label_img->val[v_index] = -1; // Vão ser substituidos quando conquistados
      dist_map[v_index] = IFT_INFINITY_DBL;
    }
  }
  printf("2\n");

  for(int s = 0; s < 2; ++s)
  {
    int s_index;
    iftVoxel s_voxel;
    s_voxel.x = X_value[s]; s_voxel.y = Y_value[s]; s_voxel.z = 0;
    s_index = iftGetVoxelIndex(label_img, s_voxel);
    label_img->val[s_index] = s+1;
    dist_map[s_index] = 0;
    root_map[s_index] = s_index;
    iftInsertDHeap(heap, s_index);
  }
  printf("3\n");

  while(!iftEmptyDHeap(heap))
  {
    int vi_index;
    iftVoxel vi_voxel;

    vi_index = iftRemoveDHeap(heap);
    vi_voxel = iftGetVoxelCoord(label_img,vi_index);

    for(int j = 1; j < A->n; ++j)
    {
      iftVoxel vj_voxel;

      vj_voxel = iftGetAdjacentVoxel(A, vi_voxel, j);

      if(iftValidVoxel(label_img, vj_voxel))
      {
        int vj_index;

        vj_index = iftGetVoxelIndex(label_img, vj_voxel);
        if(heap->color[vj_index] != IFT_BLACK) // Out of the heap?
        {
          double pathcost, arccost;
          // Função de Custo do Arco
        //   arccost = iftEuclDistance(mimg->val[root_map[vi_index]], //
        //                             mimg->val[vj_index], 
        //                             mimg->m);
                  arccost = iftEuclDistance(mimg->val[vi_index], //
                                    mimg->val[vj_index], 
                                    mimg->m);
          // Função de Custo do Caminho
          pathcost = iftMax(dist_map[vi_index], arccost); // NOPE
        // pathcost = dist_map[vi_index]+pow(arccost, 12)+iftVoxelDistance(vi_voxel,vj_voxel);

          if(pathcost < dist_map[vj_index]) // Lesser path-cost?
          {
            if(heap->color[vj_index] == IFT_GRAY) // Already within the heap?
            { iftRemoveDHeapElem(heap, vj_index); } // Remove for update

            label_img->val[vj_index] = label_img->val[vi_index];
            dist_map[vj_index] = pathcost; // Mark as conquered
            root_map[vj_index] = root_map[vi_index];
            iftInsertDHeap(heap, vj_index);
          }
        }
      }
    }
  }
  printf("4\n");
  iftDestroyMImage(&mimg);
  iftDestroyAdjRel(&A);
  iftDestroyDHeap(&heap);
  free(dist_map);
  free(root_map);

  return label_img;
}

//     int maxval;
//     iftImage *newlabel_img;

//     newlabel_img = iftCopyImage(label_img);

//     for(int p = 0; p < newlabel_img->n; ++p){
//         if(reseg_img->val[p] != 0) { newlabel_img->val[p] = IFT_INFINITY_DBL; }
//         else { newlabel_img->val[p] = IFT_INFINITY_DBL_NEG; }
//     }
//     for(int i = 0; i<2;i++)
//     {
//         iftVoxel coord;
//         coord.x = X_value[i];
//         coord.y = Y_value[i];
//         coord.z = 0;
  
//         int pos = label_img->val[iftGetVoxelIndex(label_img,coord)];
//         newlabel_img->val[pos] = 0;
//     }
//     return newlabel_img;
// }

void getCSVCoord(const iftCSV *csv, const char *value, long *X_value, long *Y_value) {

    if (csv == NULL || value == NULL || X_value == NULL || Y_value == NULL) return;
    int pos = 0;
    // Search for the value in the CSV data
    for (long row = 0; row < csv->nrows; row++) {
        if (strcmp(csv->data[row][0], value) == 0) {
            // If the value is found, extract X and Y values from the next cell in the same row
            char *x_string = csv->data[row][1];
            char *y_string = csv->data[row][2];
            // printf("%d",pos);
            X_value[pos] = strtol(x_string, NULL, 10); // Convert substring to long integer
            Y_value[pos] = strtol(y_string, NULL, 10); // Convert substring to long integer
            pos++;
        }
    }
    // Value not found in the CSV data
    // printf("Value '%s' not found in the CSV data.\n", value);
}
