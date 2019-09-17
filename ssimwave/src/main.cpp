#include <dirent.h>
#include <libavformat/avformat.h>
#include "decodeObject.h"

#define MAX_FILE_NAME_LENGTH 100
#define MAX_NUMBER_OF_FILES 100 
// get only regular files. Exclude links to avoid possible recursion
int getAllFileNames(char *dirName,char **fileNames)
{
    int fileCount=0;
    DIR *dir = opendir(dirName);
    if (dir)
    {
        for (struct dirent *ent=readdir(dir);ent;ent=readdir(dir)) 
        {
            if (ent->d_type == DT_REG)
            {
                strncpy(fileNames[fileCount],dirName,MAX_FILE_NAME_LENGTH);
                strncat(fileNames[fileCount],"/",MAX_FILE_NAME_LENGTH);
                strncat(fileNames[fileCount],ent->d_name,MAX_FILE_NAME_LENGTH);
                fileCount++;
            }
            if (fileCount > MAX_NUMBER_OF_FILES)
            {
                fprintf(stderr,"max number of files in directory %s exceeded (%d)\n",dirName,MAX_NUMBER_OF_FILES);
                exit(1);
            }
        }
        closedir(dir);
    } 
    return fileCount;
}

// 1) start numThreads operating on an array of file
// 2) analyze the set of files
// 3) return results
// 4) destroy threads 
#define MAX_NUM_THREADS 10
void dispatchThreads(char **files,double *results,int numThreads)
{
    decodeObject *obj[MAX_NUM_THREADS];
    for (int i=0;i<numThreads;i++) obj[i] = new decodeObject(files[i],i);
    for (int i=0;i<numThreads;i++) obj[i]->createThread();
    for (int i=0;i<numThreads;i++) obj[i]->joinThread();
    // will only get here after all threads are done
    for (int i=0;i<numThreads;i++) results[i] = obj[i]->getSequenceLumaAve();
    for (int i=0;i<numThreads;i++) delete obj[i];
}

void usage(char *thisProgram)
{
    fprintf(stderr,"usage: %s <number of threads> <directory of video files>\n",thisProgram);
    fprintf(stderr,"usage: where 0 < <number of threads> <= %d\n",MAX_NUM_THREADS);
    exit(1);
}

#define MIN(a,b) ( a<b ? a : b )
int main (int argc, char **argv)
{
    if (argc != 3 ) usage(argv[0]);
    int threadsToUseAtOneTime= atoi(argv[1]);
    if (threadsToUseAtOneTime>MAX_NUM_THREADS || threadsToUseAtOneTime<1) usage(argv[0]);
    char *fileNames[MAX_NUMBER_OF_FILES];
    double results[MAX_NUMBER_OF_FILES];
    for (int i=0;i<MAX_NUMBER_OF_FILES;i++) fileNames[i] = (char *)malloc(MAX_FILE_NAME_LENGTH);
    int fileCount = getAllFileNames(argv[2],fileNames);

    if (threadsToUseAtOneTime > fileCount) 
        fprintf(stderr,"threadsToUseAtOneTime > fileCount: will use only 1 thread per file, therfore fileCount\n");
    // distribute the files to the desired number of threads as many times as needed
    double *resultPtr=results;
    int threadsThisIteration;
    for (int i=0;i<fileCount;i+=threadsThisIteration)
    {
        threadsThisIteration = MIN(fileCount-i,threadsToUseAtOneTime);
        dispatchThreads(&fileNames[i],&results[i],threadsThisIteration);
        for (int j=i;j<i+threadsThisIteration;j++) printf("%s sequence average %f\n",fileNames[j],results[j]);
    }
    double mean=0.0;
    // bubble sort the result array in ascending order 
    for (int i=0;i<fileCount;i++)
    {
        for (int j=0;j<fileCount-i-1;j++)
        {
            if (results[j] > results[j+1])
            {
                double temp=results[j+1];
                results[j+1] = results[j];
                results[j] = temp;
            }
        }
    }
    for (int i=0;i<fileCount;i++) mean += results[i];
    mean /= fileCount;
    double median = (fileCount & 1) ? results[fileCount>>1] : (results[fileCount>>1] + results[(fileCount>>1)-1])/2;
    printf("stat summary of all clips:\n");
    printf("min %f max %f mean %f median %f\n",results[0],results[fileCount-1],mean,median);
}
