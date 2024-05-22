/*****************************************************************************\
* RunRndVoronoy.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2024-03-26
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"

/* HEADERS __________________________________________________________________*/
void usage();
void readImgSize
(iftArgs *args, int *num_seeds, int *width, int *height, int *depth, const char **path);
iftImage *iftRndVoronoy
(int width, int height, int depth, int num_seeds);

/* MAIN _____________________________________________________________________*/
int main(int argc, char const *argv[])
{
  /*-------------------------------------------------------------------------*/
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);
  has_req = iftExistArg(args, "numseeds") && //
            iftExistArg(args, "width") && iftExistArg(args, "height") && //
            iftExistArg(args, "out");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  { usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
  /*-------------------------------------------------------------------------*/
  const char *OUT_PATH;
  int num_seeds, width, height, depth;
  iftImage *out_img;

  readImgSize(args, &num_seeds, &width, &height, &depth, &OUT_PATH);
  iftDestroyArgs(&args);

  out_img = iftRndVoronoy(width, height, depth, num_seeds);

  if(depth == 1)
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
  printf("%-*s %s\n", SKIP_IND, "--numseeds", 
         "Quantity of randomly sampled seeds");
  printf("%-*s %s\n", SKIP_IND, "--width", 
         "Image width");
  printf("%-*s %s\n", SKIP_IND, "--height", 
         "Image height");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output label image");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--depth", 
         "Image depth");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

void readImgSize
(iftArgs *args, int *num_seeds, int *width, int *height, int *depth, const char **path)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(args != NULL);
  assert(num_seeds != NULL);
  assert(width != NULL);
  assert(height != NULL);
  assert(depth != NULL);
  assert(path != NULL);
  #endif /*##################################################################*/
  if(iftHasArgVal(args, "numseeds")) 
  {
    (*num_seeds) = atoi(iftGetArg(args, "numseeds"));
  }
  else { iftError("No quantity of seeds was given", __func__); }

  if(iftHasArgVal(args, "width")) 
  {
    (*width) = atoi(iftGetArg(args, "width"));
  }
  else { iftError("No image width was given", __func__); }

  if(iftHasArgVal(args, "height")) 
  {
    (*height) = atoi(iftGetArg(args, "height"));
  }
  else { iftError("No image height was given", __func__); }

  if(iftExistArg(args, "depth")) 
  {
    if(iftHasArgVal(args, "depth") == true) 
    { (*depth) = atoi(iftGetArg(args, "depth")); }
    else { iftError("No image depth was given", __func__); }
  }
  else { (*depth) = 1; }

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);
}

iftImage *iftRndVoronoy
(int width, int height, int depth, int num_seeds)
{
  srand(time(NULL));
  int s_count;
  double *dist_map;
  iftAdjRel *A;
  iftDHeap *heap;
  iftImage *label_img;

  label_img = iftCreateImage(width, height, depth);
  dist_map = calloc(label_img->n, sizeof(double));
  if(iftIs3DImage(label_img) == true) 
  { A = iftSpheric(sqrtf(1.0)); }
  else { A = iftCircular(sqrtf(1.0)); }

  heap = iftCreateDHeap(label_img->n, dist_map);
  iftSetRemovalPolicyDHeap(heap, MINVALUE);

  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for
  #endif //-------------------------------------------------------------------|
  for(int v_index = 0; v_index < label_img->n; ++v_index)
  {
    label_img->val[v_index] = -1;
    dist_map[v_index] = IFT_INFINITY_DBL;
  }

  s_count = 0;
  while(s_count < num_seeds)
  {
    int s_index;

    s_index = iftRandomInteger(0, label_img->n - 1);
    if(dist_map[s_index] == IFT_INFINITY_DBL)
    {
      s_count++;
      label_img->val[s_index] = s_count;
      dist_map[s_index] = 0;
      iftInsertDHeap(heap, s_index); 
    } 
  } // Add seeds

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
          double pathcost;

          pathcost = dist_map[vi_index] + iftVoxelDistance(vi_voxel, vj_voxel);

          if(pathcost < dist_map[vj_index]) // Lesser path-cost?
          {
            if(heap->color[vj_index] == IFT_GRAY) // Already within the heap?
            { iftRemoveDHeapElem(heap, vj_index); } // Remove for update

            label_img->val[vj_index] = label_img->val[vi_index];
            dist_map[vj_index] = pathcost; // Mark as conquered
            iftInsertDHeap(heap, vj_index);
          }
        }
      }
    }
  }
  iftDestroyAdjRel(&A);
  iftDestroyDHeap(&heap);
  free(dist_map);

  return label_img;
}