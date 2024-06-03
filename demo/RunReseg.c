/*****************************************************************************\
* RunReseg.c
*
* AUTHOR  : Lucca Lacerda
* DATE    : 2024-04-17
* LICENSE : MIT License
* EMAIL   : lsplacerda@sga.pucminas.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"
#include "iftMetrics.h"

/* HEADERS __________________________________________________________________*/
void usage();
void readImgInputs
(iftArgs *args, iftImage **img, iftImage **segm, iftCSV **csv, const char **path, 
  bool *is_video);
iftImage *segMultiScale
(iftImage *segm, long *X_value, long *Y_value);
void getCSVCoord(const iftCSV *csv, const char *label, long *X_value, long *Y_value);
iftVoxel getVoxelCentroide(iftImage *img, int label);
void freezeSP(iftImage *img, int scale, int label, int value);
bool hasNonVisitedSP(int *visitList, int size);
void checkAsVisitedSP(iftImage *img, int labelS1, int labelS2);
void getNewS(iftImage *img, int *labelS1, int *labelS2);
void swapLabel(iftImage *img, int label0, int label1, int scale);
void cropImageAsLayer(iftImage *img, int layer, iftVoxel coord );
bool isValidLabel(iftImage *img, int label);
void findS(iftImage *img, int *label);
void getAdj(iftImage *img, int **adjM, int *visitList);
int **allocMatrix (int m, int n);
iftVoxel findFurthestNeighborAdj(iftImage *img, iftVoxel *centroides, int size, int **adjM, iftVoxel coordS1);

/* MAIN _____________________________________________________________________*/
int main(int argc, char const *argv[])
{
  /*-------------------------------------------------------------------------*/
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);
  has_req = iftExistArg(args, "img") && iftExistArg(args, "out") && iftExistArg(args, "segm") && iftExistArg(args,"file");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  { usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
  /*-------------------------------------------------------------------------*/
  const char *OUT_PATH;
  bool is_video;
  iftImage *img, *out_img, *segm;
  iftCSV *csv;
  long X_value[2];
  long Y_value[2];

  readImgInputs(args, &img, &segm, &csv, &OUT_PATH, &is_video);
  getCSVCoord(csv, "Seed", X_value, Y_value); 

  iftDestroyArgs(&args);

  out_img = segMultiScale(segm, X_value, Y_value);
  iftDestroyImage(&img);
  iftDestroyImage(&segm);
  is_video = 0;
  if(!is_video)
  { 
    // printf("Write img\n");
    iftWriteImageByExt(out_img, OUT_PATH); 
  }
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
  printf("%-*s %s\n", SKIP_IND, "--img", 
         "Input image");
  printf("%-*s %s\n", SKIP_IND, "--segm", 
         "Directory with multiscale segmentated images");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output resegmented image");
    printf("%-*s %s\n", SKIP_IND, "--file", 
         "File with the seeds coordenates information ");



  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

void readImgInputs
(iftArgs *args, iftImage **img, iftImage **segm, iftCSV **csv, const char **path, 
  bool *is_video)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(args != NULL);
  assert(img != NULL);
  assert(path != NULL);
  #endif //------------------------------------------------------------------//
  const char *PATH;

  if(iftHasArgVal(args, "img") == true) 
  {
    PATH = iftGetArg(args, "img");

    if(iftIsImageFile(PATH) == true) 
    { (*img) = iftReadImageByExt(PATH); (*is_video) = false; }
    else if(iftDirExists(PATH) == true)
    { (*img) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); } 
  }
  else iftError("No label image path was given", __func__);

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
   else iftError("No output image path was given", __func__);

  if(iftExistArg(args, "segm") == true)
  {
    if(iftHasArgVal(args, "segm") == true) 
    {
      PATH = iftGetArg(args, "segm");

      if(iftIsImageFile(PATH) == true) 
      { 
        // printf("IMAGEM\n");
        (*segm) = iftReadImageByExt(PATH);
        (*is_video) = false; 
      }
      else if(iftDirExists(PATH) == true)
      { 
        // printf("DIRETORIO\n");
        (*segm) = iftReadImageFolderAsVolume(PATH);
        (*is_video) = true;
      }
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

iftImage *segMultiScale
(iftImage *segm, long *X_value, long *Y_value)
{
  iftImage *label_img, *visited;
  iftVoxel coordS1, coordS2;
  //  centroideS1, centroideS2;


  int labelS1 = -1, 
      labelS2 = -1,
      min = IFT_INFINITY_INT_NEG,
      max = IFT_INFINITY_INT,
      size = 0;

  label_img = iftCreateImage(segm->xsize, segm->ysize, 1);

  cropImageAsLayer(segm,4,coordS1);

  segm = iftRelabelImage(segm);
  iftMinMaxValues(segm,&min,&max); 


  size = max + 1;

  double *prioS1 = (double *) calloc (size, sizeof(double));
  double *prioS2 = (double *) calloc (size, sizeof(double));
  
  iftDHeap *f1 = iftCreateDHeap(size, prioS1),
           *f2 = iftCreateDHeap(size, prioS2);

  coordS1.x = X_value[0];
  coordS1.y = Y_value[0];
  coordS1.z = 0;
  coordS2.x = X_value[1];
  coordS2.y = Y_value[1];
  coordS2.z = 0;

  //   printf("Voxel S1 x: %d y:%d z:%d\n", coordS1.x,coordS1.y, coordS1.z);
  //   printf("Voxel S2 x: %d y:%d z:%d\n", coordS2.x,coordS2.y, coordS2.z);
  // printf("labelS1 %d labelS2 %d\n", labelS1, labelS2); 
  int l = 0;
  visited = iftGetXYSlice(segm,l);


  int **adjM = allocMatrix(size,size);
  
  int *visitList = (int *) calloc (size, sizeof(int));

  iftVoxel *centroides = (iftVoxel *) calloc (size, sizeof(iftVoxel));
  
  for(int i = 0; i < size;i++){
    centroides[i] = getVoxelCentroide(visited,i);
  }

  // printf("alloc\n");
  getAdj(visited, adjM, visitList);
  // printf("adjacency\n");

  // while(hasNonVisitedSP(visited))
  // // while(false)
  // {
  //   // printf("Nx %d\n",quant);
  for (int i = 0; i < size; i++)
  {
    labelS1 = segm->val[iftGetVoxelIndex(segm,coordS1)];
    labelS2 = segm->val[iftGetVoxelIndex(segm,coordS2)];
    iftInsertDHeap(f1,labelS1);
    iftInsertDHeap(f2,labelS2);


    /*
    While(...){

      remove um spx S
      marca S como parte de S1 ou S2
      para cada spx vizinho de S:
    } 
    
    */

   while(hasNonVisitedSP(visitList, size)){
    iftRemoveDHeapElem(f1,labelS1);
    iftRemoveDHeapElem(f2,labelS2);
    visitList[labelS1] = 0;
    visitList[labelS2] = 0;
    coordS1 = findFurthestNeighborAdj(segm, centroides, size, adjM, coordS1);
    coordS2 = findFurthestNeighborAdj(segm, centroides, size, adjM, coordS2);
    labelS1 = segm->val[iftGetVoxelIndex(segm,coordS1)];
    labelS2 = segm->val[iftGetVoxelIndex(segm,coordS2)];
   }
  }
  
  min = IFT_INFINITY_INT_NEG;
  max = IFT_INFINITY_INT;
  iftMinMaxValues(label_img,&min,&max); 
  
  printf("Min %d Max %d\n", min, max);
  free(centroides);
  free(adjM);
  
  return label_img;
}


iftVoxel findFurthestNeighborAdj(iftImage *img, iftVoxel *centroides, int size, int **adjM, iftVoxel coord)
{
  double dist = IFT_INFINITY_DBL_NEG;
  double aux;
  iftVoxel resp;
  resp.x = 0;
  resp.y = 0;
  resp.z = 0;
  for (int i = 0; i < size; i++)
  {
      aux = iftEuclDistance(img->val[iftGetVoxelIndex(img, centroides[i])], //
                                    img->val[iftGetVoxelIndex(img,coord)], 
                                    img->n);
      printf("%f\n", aux);
      if(aux > dist && adjM[img->val[iftGetVoxelIndex(img, centroides[i])]][img->val[iftGetVoxelIndex(img,coord)]] == 1){
        resp = centroides[i];
        dist = aux;
      }
  }
  return resp;

}


int **allocMatrix (int m, int n)// Retirado de https://www.pucsp.br/~so-comp/cursoc/aulas/ca70.html
{
  int **v;  /* ponteiro para a matriz */
  int   i;    /* variavel auxiliar      */
  if (m < 1 || n < 1) { /* verifica parametros recebidos */
     printf ("** Erro: Parametro invalido **\n");
     return (NULL);
     }
  /* aloca as linhas da matriz */
  v = (int **) calloc (m, sizeof(int *));	/* Um vetor de m ponteiros para int */
  if (v == NULL) {
     printf ("** Erro: Memoria Insuficiente **");
     return (NULL);
     }
  /* aloca as colunas da matriz */
  for ( i = 0; i < m; i++ ) {
      v[i] = (int*) calloc (n, sizeof(int));	/* m vetores de n floats */
      if (v[i] == NULL) {
         printf ("** Erro: Memoria Insuficiente **");
         return (NULL);
         }
      }
  return (v); /* retorna o ponteiro para a matriz */
}



void cropImageAsLayer(iftImage *img, int layer, iftVoxel coord ){
  coord.z = layer;
  int val = img->val[iftGetVoxelIndex(img,coord)];

  iftVoxel p;
    for(coord.y = 0; coord.y < img->ysize;coord.y++)
    {
      for(coord.x = 0; coord.x < img->xsize;coord.x++)
      {
        if(img->val[iftGetVoxelIndex(img,coord)] != val)
          {
            // printf("%d\n",p.z);
            p.x = coord.x;
            p.y = coord.y;
            for(p.z = 0; p.z < img->zsize;p.z++)
            {
              img->val[iftGetVoxelIndex(img,p)] = 0;
            }
          }
      }
    }
}



void swapLabel(iftImage *img, int label0, int label1, int scale)
{
  int min = IFT_INFINITY_INT, max = IFT_INFINITY_INT_NEG;
  iftMinMaxValues(img,&min,&max);
  iftVoxel p;
  p.z = scale;
  for(p.y = 0; p.y < img->ysize;p.y++)
  {
    for(p.x = 0; p.x < img->xsize;p.x++)
    {
      if(img->val[iftGetVoxelIndex(img,p)] == label0)
        img->val[iftGetVoxelIndex(img,p)] = label1;
      else
        if(img->val[iftGetVoxelIndex(img,p)] == label1)
          img->val[iftGetVoxelIndex(img,p)] = label0;
        // else
          // printf("%d\n",label0);
    }
  }
}

void findS(iftImage *img, int *label)
  {
  printf("Entrou como = %d\n",*label);
  int newLabel = -1;
  int min = IFT_INFINITY_INT, max = IFT_INFINITY_INT_NEG;
  iftMinMaxValues(img,&min,&max);
  // int mean = (min + max)/2;
  
  
  newLabel = *label;
  do{
    if(newLabel < max)
    {
      newLabel++;
    }else{
      newLabel--;
    } 
    // printf("label %d new %d\n",*label, newLabel);
  } while (!isValidLabel(img,newLabel) || newLabel == 0 || newLabel > max);

  while (!isValidLabel(img,newLabel) || newLabel == 0 || newLabel > max){
    if(newLabel > min)
    {
      newLabel--;
    }else{
      newLabel++;
    } 
    // printf("label %d new %d\n",label, newLabel);
  }

  *label = newLabel;
  printf("Saiu como = %d\n",*label);
}



void getNewS(iftImage *img, int *labelS1, int *labelS2)
{
  iftImage *visited = iftCopyImage(img);
  findS(img,labelS1);
  checkAsVisitedSP(visited,*labelS1,*labelS1);
  findS(visited,labelS2);
  iftDestroyImage(&visited);
}

bool isValidLabel(iftImage *img, int label)
{
  bool resp = false;
  iftVoxel p;
  p.z = 0;
  for(p.y = 0; p.y < img->ysize;p.y++)
  {
    for(p.x = 0; p.x < img->xsize;p.x++)
    {
      if(img->val[iftGetVoxelIndex(img,p)] == label){
        printf("X = %d Y = %d\n",p.x,p.y);
        return true;
      }
        
    }
  }

  return resp;
}

void getAdj(iftImage *img, int **adjM, int *visitList){
  
  int label1, label2;
  iftVoxel p, aux;
  p.z = 4;
  aux.z = 4;
  for(p.y = 0; p.y < img->ysize;p.y++)
  {
    for(p.x = 0; p.x < img->xsize;p.x++)
    {
      // printf("XSize- %d -YSize- %d -X- %d -Y- %d\n", img->xsize, img->ysize, p.x, p.y);
      if(p.x > 0)
      {
        aux.x = p.x-1;
        aux.y = p.y;
        label1 = img->val[iftGetVoxelIndex(img,p)];
        label2 = img->val[iftGetVoxelIndex(img,aux)];
        // printf("Primeira label 1: %d label 2: %d\n",label1,label2);
        if(p.x > 0 && label1 != label2)
        {
          adjM[label1][label2] = 1;
          adjM[label2][label1] = 1;
          visitList[label1] = 1;
          visitList[label2] = 1;
        }
      }
      if(p.y > 0)
      {
        aux.x = p.x;
        aux.y = p.y-1;
        label1 = img->val[iftGetVoxelIndex(img,p)];
        label2 = img->val[iftGetVoxelIndex(img,aux)];
        // printf("Segunda label 1: %d label 2: %d\n",label1,label2);
        if(p.x > 0 && label1 != label2)
        {
          adjM[label1][label2] = 1;
          adjM[label2][label1] = 1;
          visitList[label1] = 1;
          visitList[label2] = 1;
        }
      }
      if(p.x < img->xsize-1)
      {
        aux.x = p.x+1;
        aux.y = p.y;
        label1 = img->val[iftGetVoxelIndex(img,p)];
        label2 = img->val[iftGetVoxelIndex(img,aux)];
        // printf("Terceira label 1: %d label 2: %d\n",label1,label2);
        if(p.x > 0 && label1 != label2)
        {
          adjM[label1][label2] = 1;
          adjM[label2][label1] = 1;
          visitList[label1] = 1;
          visitList[label2] = 1;
        }
      }
      if(p.y > img->ysize-1)
      {
        aux.x = p.x;
        aux.y = p.y+1;
        label1 = img->val[iftGetVoxelIndex(img,p)];
        label2 = img->val[iftGetVoxelIndex(img,aux)];
        // printf("Quarta label 1: %d label 2: %d\n",label1,label2);
        if(p.x > 0 && label1 != label2)
        {
          adjM[label1][label2] = 1;
          adjM[label2][label1] = 1;
          visitList[label1] = 1;
          visitList[label2] = 1;
        }
      }
    }
  }

}

bool hasNonVisitedSP(int *visitList, int size)
{
  bool resp = false;
  for(int x = 0; x < size;x++)
  {
    if(visitList[x] != 0)
    {
      // printf("Não visitado %d\n", img->val[iftGetVoxelIndex(img,p)]);
      resp = true;
      return resp;
    }   
  }
  return resp;
}

void checkAsVisitedSP(iftImage *img, int labelS1, int labelS2)
{
  iftVoxel p;
  p.z = 0;
  for(p.y = 0; p.y < img->ysize;p.y++)
  {
    for(p.x = 0; p.x < img->xsize;p.x++)
    {
      if(img->val[iftGetVoxelIndex(img,p)] == labelS1 || img->val[iftGetVoxelIndex(img,p)] == labelS2){
        // printf("LabelS1 %d LabelS2 %d Valor excluido %d\n", labelS1, labelS2, img->val[iftGetVoxelIndex(img,p)] );
        img->val[iftGetVoxelIndex(img,p)] = -1;   
      }
    }
  }
}

void freezeSP(iftImage *img, int scale, int label,int value)
{
  // printf("Entrou\n");
  iftVoxel p;
  p.z = scale;

  for(p.y = 0; p.y < img->zsize;p.y++)
  {
    for(p.x = 0; p.x < img->zsize;p.x++)
    {
      if(img->val[iftGetVoxelIndex(img,p)] == label)
        img->val[iftGetVoxelIndex(img,p)] = value;   
    }
  }
  // printf("Saiu\n");
}


iftVoxel getVoxelCentroide(iftImage *img, int label)
{
  iftVoxel resp, aux;
  int sumX = 0, sumY = 0, quant = 0;
  aux.z = 0;
  for (aux.y = 0; aux.y < img->ysize; aux.y++)
  {
      for (aux.x = 0; aux.x < img->xsize; aux.x++)
      {
        if(img->val[iftGetVoxelIndex(img, aux)] == label){
          sumX += aux.x;
          sumY += aux.y;
          quant ++;
          // printf("soma do X %d Y %d\n",sumX,sumY);
        }
      }
      // printf("img->ysize = %d  and y = %d\n", img->ysize, y);
  }
  resp.x = sumX/(float)quant;
  resp.y = sumY/(float)quant;
  resp.z = 0;
  return resp;
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



/*

Criar a lista de visitados
alterar metodo de visitação
testar se ele esta funcionando
setar a nova forma de gerar S1 e S2
*/



//   aux1 = labelS1;
  //   aux2 = labelS2;

  //   // freezeSP(segm,0,aux1,-1);
  //   // freezeSP(segm,0,aux2,-2);
  //   // centroideS1 = getVoxelCentroide(segm,labelS1);
  //   // centroideS2 = getVoxelCentroide(segm,labelS2);
  //   while(aux1 != aux2 && l < segm->zsize)
  //   {
  //     // printf("%d scale %d segm->zsize\n", l, segm->zsize);
  //     // slice = iftGetXYSlice(segm,l);
  //     l++;
  //     coordS1.z = coordS2.z = l;

  //     printf("Camada %d FREEZOU\n", l);
  //     freezeSP(segm,(l-1),aux1,-1);
  //     freezeSP(segm,(l-1),aux2,-2);
     
  //     printf("Ift get Voxel coordS1 %d %d %d\n", coordS1.x, coordS1.y, coordS1.z);
  //     aux1 = segm->val[iftGetVoxelIndex(segm,coordS1)];
  //     printf("Ift get Voxel coordS2 %d %d %d\n", coordS2.x, coordS2.y, coordS2.z);
  //     aux2 = segm->val[iftGetVoxelIndex(segm,coordS2)];
  //     printf("freeze\n");
  //   }
  //   // printf("\n");
  //   // printf("labelS1 %d labelS2 %d\n", labelS1, labelS2);
    
  //   printf("Check as visited %d %d \n", labelS1, labelS2);
  //   checkAsVisitedSP(visited,labelS1,labelS2);
    
  //   printf("GET NEW S\n");
  //   getNewS(visited, &labelS1, &labelS2);

  //   printf("Centroide S1 de label %d e S2 de label %d\n", labelS1, labelS2);
  //   coordS1 = getVoxelCentroide(visited,labelS1);
  //   printf("-----------------------------\n");
  //   coordS2 = getVoxelCentroide(visited,labelS2);
  //   // printf("labelS1 %d labelS2 %d\n", labelS1, labelS2);
  //   // printf("final\n");
  //   quant++;
  // }
  // printf("quantidade %d\n", quant);
  // for(int z = 0; z < segm->zsize;z++)
  // {
  //   // swapLabel(segm,-1,1, z);
  //   // swapLabel(segm,-2,2, z);
  //   // swapLabel(segm,IFT_INFINITY_INT_NEG,0,z);
  // }
  // label_img = iftGetXYSlice(segm,4);
  
  
  // for(int i = 0; i < size; i++)
  // {
  //   for(int j = 0; j < size; j++)
  //   {
  //     printf("%d ",adjM[i][j]);  
  //   }
  //   printf("\n");  
  // }


  // for(int j = 0; j < size; j++)
  // {
  //   printf("%d: X - %d Y - %d Z- %d\n",j, centroides[j].x,centroides[j].y,centroides[j].z);  
  // }