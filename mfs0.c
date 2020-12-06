// The MIT License (MIT)
// 
// Copyright (c) 2020 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_NUM_ARGUMENTS 10

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

FILE *fp;

struct __attribute__((__packed__)) DirectoryEntry
{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4];
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;

};

struct DirectoryEntry Dir[16];
  
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x01
#define ATTR_SYSTEM 0x04
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

uint16_t BPB_BytesPerSec;
uint8_t BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t BPB_NumFATS;
uint32_t BPB_FATSz32;
int32_t currDirectory;

int16_t NextLB(uint32_t sector)
{
    uint32_t FATAddress = (BPB_BytesPerSec*BPB_RsvdSecCnt)+(sector*4);
    int16_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 2, 1, fp);
    return val;
}

int LBAToOffset(int32_t sector)
{
    return ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);
}

int compare(char *user, char *directory)
{
    char *spacing = "..";

    if(strncmp(spacing, user, 2) == 0)
    {
        if(strncmp(user, directory, 2) == 0)
        {
            return 1;
        }
            return 0;
    }

    char IMG_Name[12];
    strncpy(IMG_Name,directory,11);
    IMG_Name[11] = '\0';
    char input[11];
    memset(input, 0, 11);
    strncpy(input, user, strlen(user));
    char expanded_name[12];
    memset(expanded_name, ' ', 12);
    char *token = strtok(input, ".");
    strncpy(expanded_name, token, strlen(token));
    token = strtok(NULL, ".");

    if(token)
    {
        strncpy((char*)(expanded_name + 8), token, strlen(token));
    }

    int i;

    for(i = 0; i < 11; i++)
    {
        expanded_name[i] = toupper(expanded_name[i]);
    }

    if(strncmp(expanded_name, IMG_Name, 11) == 0)
    {
        return 1;
    }

    return 0;
}


int readfile( char *filename, int requested_Offset, int requestedBytes)
{
    int i;
    int got=0;
    int bytesRemainingToRead = requestedBytes;
    
    if(requested_Offset < 0)
    {
        printf("Error: offset can't be negative\n");
    }

    for(i = 0; i < 16; i++)
    {
        if(compare(filename, Dir[i].DIR_Name))
        {
            int cluster = Dir[i].DIR_FirstClusterLow;
            got = 1;

            int SearchSize = requested_Offset;

            while(SearchSize >= BPB_BytesPerSec)
            {
                cluster = NextLB( cluster );
                SearchSize = SearchSize - BPB_BytesPerSec;
            }

            int offset = LBAToOffset( cluster );
            int byteOffset = ( requested_Offset % BPB_BytesPerSec );
            fseek(fp, offset + byteOffset, SEEK_SET);

            //printf("cluster %d\n", cluster);

            unsigned char buffer [ BPB_BytesPerSec ];

            int firstBlockBytes = BPB_BytesPerSec - requested_Offset;

            fread(buffer, 1, firstBlockBytes, fp);
            //fread(buffer,1,byteOffset,fp);

            for(i = 0; i < firstBlockBytes; i++)
            {
                printf("%x ", buffer[i]);
            }
            
            bytesRemainingToRead = bytesRemainingToRead - firstBlockBytes;

            while(bytesRemainingToRead >= 512)
            {
                cluster = NextLB( cluster );
                offset = LBAToOffset( cluster );
                fseek(fp, offset , SEEK_SET);
                fread(buffer, 1, BPB_BytesPerSec, fp);

                for(i = 0; i < BPB_BytesPerSec; i++)
                {
                    printf("%x ", buffer[i]);
                }
            bytesRemainingToRead = bytesRemainingToRead - BPB_BytesPerSec;
            }

            if( bytesRemainingToRead)
            {
                cluster = NextLB( cluster );
                offset = LBAToOffset( cluster );
                fseek(fp, offset, SEEK_SET);
                fread(buffer, 1, bytesRemainingToRead, fp);
            
                for(i=0; i< bytesRemainingToRead; i++)
                {
                    printf("%x ", buffer[i]);
                }
            }
            
            printf("\n");
        }
    }

    if(!got)
    {
        printf("Error: File not found\n");
        return -1;
    }
}


int getFile(char *olderfilename, char *newfilename)
{

    FILE *oldpointer;

    if(newfilename == NULL)
    {
        oldpointer = fopen(olderfilename,"w");
        if(oldpointer == NULL)
        {
            printf("Error: Cant open new file %s\n", olderfilename);
        }
    }
    else
    {
        oldpointer = fopen(newfilename,"w");

        if(oldpointer == NULL)
        {
            printf("Error: Cant open new file %s\n", newfilename);
        }
    }

    int i;
    int got=0;

    for(i = 0; i < 16 ; i++)
    {
        if(compare(olderfilename, Dir[i].DIR_Name ) )
        {
            int cluster = Dir[i].DIR_FirstClusterLow;
            got =1;
            int byteremainingtoread = Dir[i].DIR_FileSize;
            int offset=0;
            unsigned char buffer[512];

            while(byteremainingtoread >= BPB_BytesPerSec)
            {
                offset = LBAToOffset(cluster);
                fseek(fp, offset, SEEK_SET);
                fread(buffer, 1, BPB_BytesPerSec, fp);
                fwrite(buffer, 1, 512, oldpointer);
                cluster = NextLB(cluster);
                byteremainingtoread = byteremainingtoread - BPB_BytesPerSec;
            }

            //Handle the last block

            if(byteremainingtoread)
            {
                offset = LBAToOffset(cluster);
                fseek(fp, offset, SEEK_SET);
                fread(buffer, 1, byteremainingtoread,fp);
                fwrite(buffer,1,byteremainingtoread,oldpointer);
            }
            fclose(oldpointer);
        }
    }
}




int main()
{
    bool isOpen = false;
    
    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

    while( 1 )
    {
        // Print out the mfs prompt
        printf ("mfs> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

        if (cmd_str[0] == '\n')
        {
            continue;
        }

        /* Parse input */
        char *token[MAX_NUM_ARGUMENTS];

        int   token_count = 0;                                 
                                                            
        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;                                         
                                                            
        char *working_str  = strdup( cmd_str );                

        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;

        // Tokenize the input stringswith whitespace used as the delimiter
        while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
                (token_count<MAX_NUM_ARGUMENTS))
        {
            token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
            if( strlen( token[token_count] ) == 0 )
            {
            token[token_count] = NULL;
            }
            token_count++;
        }

        // Now print the tokenized input as a debug check
        // \TODO Remove this code and replace with your FAT32 functionality

        if (strcmp("open", token[0]) == 0)
        {
            if (fp != NULL)
            {
                isOpen = true;
                printf("ERROR: File System image already open.\n");
                continue;
            }

            else
            {
                if ((fp = fopen(token[1], "r")) == NULL)
                {
                    printf("Error: File system image not found.\n");
                    continue;
                }

                fseek(fp, 11, SEEK_SET);
                fread(&BPB_BytesPerSec, 2, 1, fp);

                fseek(fp, 13, SEEK_SET);
                fread(&BPB_SecPerClus, 1, 1, fp);

                fseek(fp, 14, SEEK_SET);
                fread(&BPB_RsvdSecCnt, 2, 1, fp);

                fseek(fp, 16, SEEK_SET);
                fread(&BPB_NumFATS, 1, 2, fp);

                fseek(fp, 36, SEEK_SET);
                fread(&BPB_FATSz32, 4, 1, fp);  

                int root = (BPB_RsvdSecCnt * BPB_BytesPerSec) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);

                // fseek(fp,0x100400,SEEK_SET);
                fseek(fp,root,SEEK_SET);

                fread(Dir,sizeof(struct DirectoryEntry),16,fp);
                //printf("%x\n",root);
                
            }
        }

        else if (strcmp("close", token[0]) == 0)
        {
            if (fp != NULL)
            {
                fclose(fp);
                
                fp = NULL;
            }

            else
            {
                printf("Error: File system not open. \n");
            }
        }

        
        else if (strcmp("bpb", token[0]) == 0)
        {
            if(fp == NULL)
            {
                printf("ERROR: File System image must be opened first.\n");
            }

            else
            {
                printf("BPB_BytesPerSec(Decimal): %d\n", BPB_BytesPerSec);
                printf("BPB_BytesPerSec(HexaDecimal): 0x%x\n", BPB_BytesPerSec);

                printf("BPB_SecPerClus(Decimal): %d\n", BPB_SecPerClus);
                printf("BPB_SecPerClus (HexaDecimal): 0x%x\n", BPB_SecPerClus);

                printf("BPB_RsvdSecCnt (Decimal): %d\n", BPB_RsvdSecCnt);
                printf("BPB_RsvdSecCnt (HexaDecimal): 0x%x\n", BPB_RsvdSecCnt);

                printf("BPB_NumFATS (Decimal): %d\n", BPB_NumFATS);
                printf("BPB_NumFATS (HexaDecimal): 0x%x\n", BPB_NumFATS);

                printf("BPB_FATSz32 (Decimal): %d\n", BPB_FATSz32);
                printf("BPB_FATSz32 (HexaDecimal): 0x%x\n", BPB_FATSz32);  
            }

        }

        else if (strcmp("ls", token[0]) == 0)
        {
            if(fp == NULL)
            {
                printf("ERROR: File System image must be opened first.\n");

            }

            else
            {
            int i;
            
                for (i = 0; i < 16; i++)
                {
                    char filename[12];
                    strncpy(filename, Dir[i].DIR_Name, 11);
                    filename[11]='\0';

                    if ((Dir[i].DIR_Attr == ATTR_READ_ONLY || Dir[i].DIR_Attr ==ATTR_DIRECTORY 
                    || Dir[i].DIR_Attr == ATTR_ARCHIVE) && filename[0] != 0xffffffe5)
                    {
                        printf("%s\n", filename );
                    }

                }
            }  

            
        }

        else if (strcmp("cd", token[0]) == 0)
        {
            if(fp == NULL)
            {
                printf("ERROR: File System image must be opened first.\n");
            }

            else
            {
                int i;
                int got =0;
                
                for (i = 0; i < 16; i++)
                {
                    if(compare(token[1], Dir[i].DIR_Name))
                    {
                        int cluster = Dir[i].DIR_FirstClusterLow;
                        if(cluster == 0)
                        {
                            cluster = 2;
                        }
                        
                        int offset = LBAToOffset(cluster);
                        fseek(fp,offset,SEEK_SET);
                        fread(Dir, sizeof(struct DirectoryEntry), 16, fp);
                        got=1;
                        break;
                    }
                }

                if(!got)
                {
                    printf("Error: Directory not found\n");
                }
            }
        }


        else if (strcmp("read", token[0]) == 0)
        {
            //int requested_bytes, requested_Offset;

            if(fp == NULL)
            {
                printf("ERROR: File System image must be opened first.\n");
            }

            if(fp != NULL)
            {
                readfile( token[1], (int) atoi( token[2] ), (int)atoi( token[3] ));
            }
        }

        else if (strcmp("stat", token[0]) == 0)
        {
            if(fp == NULL)
            {
                printf("ERROR: File System image must be opened first.\n");
            }

            if(fp != NULL)
            {
                int i;
                int found =0;

                for(i=0; i<16; i++)
                { 
                    if(compare((token[1]), Dir[i].DIR_Name))
                    {
                        printf("%s Attr: %d Size: %d Cluster: %d\n", token[1], Dir[i].DIR_Attr,Dir[1].DIR_FileSize,Dir[i].DIR_FirstClusterLow);
                        found =1 ;
                    }
                }

                if(!found)
                {
                    printf("Error: File not found\n");
                }
            }
        }

        else if (strcmp("get", token[0]) == 0)
        {
            //int requested_bytes, requested_Offset;

            if(fp == NULL)
            {
                printf("ERROR: File System image must be opened first.\n");
            }

            if(fp != NULL)
            {
                getFile(token[1], token[2]);
            }
        }

        else if ((strcmp("quit", token[0]) == 0) || (strcmp("exit", token[0]) == 0))
        {
            printf("Closing the Fat32 System..\n");
            fp = NULL;
            break;
        }
    }

    return 0;
}

