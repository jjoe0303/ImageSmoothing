#include <mpi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include "bmp.h"
#include <stdlib.h>

using namespace std;

//定義平滑運算的次數
#define NSmooth 1000

/*********************************************************/
/*變數宣告：                                             */
/*  bmpHeader    ： BMP檔的標頭                          */
/*  bmpInfo      ： BMP檔的資訊                          */
/*  **BMPSaveData： 儲存要被寫入的像素資料               */
/*  **BMPData    ： 暫時儲存要被寫入的像素資料           */
/*********************************************************/
BMPHEADER bmpHeader;                        
BMPINFO bmpInfo;
RGBTRIPLE **BMPSaveData = NULL;                                               
RGBTRIPLE **BMPData = NULL;                                                   

/*********************************************************/
/*函數宣告：                                             */
/*  readBMP    ： 讀取圖檔，並把像素資料儲存在BMPSaveData*/
/*  saveBMP    ： 寫入圖檔，並把像素資料BMPSaveData寫入  */
/*  swap       ： 交換二個指標                           */
/*  **alloc_memory： 動態分配一個Y * X矩陣               */
/*********************************************************/
int readBMP( char *fileName);        //read file
int saveBMP( char *fileName);        //save file
void swap(RGBTRIPLE *a, RGBTRIPLE *b);
RGBTRIPLE **alloc_memory( int Y, int X );        //allocate memory

int main(int argc,char *argv[])
{
/*********************************************************/
/*變數宣告：                                             */
/*  *infileName  ： 讀取檔名                             */
/*  *outfileName ： 寫入檔名                             */
/*  startwtime   ： 記錄開始時間                         */
/*  endwtime     ： 記錄結束時間                         */
/*********************************************************/
	char *infileName = "input.bmp";
    char *outfileName1 = "output1.bmp";
	char *outfileName2 = "output2.bmp";
	double startwtime = 0.0, endwtime=0;
	
	
	
	/*MPI variable*/
	int nproc,myid=0;
	
	/*MPI Initialize*/
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD,&nproc);
	MPI_Comm_rank(MPI_COMM_WORLD,&myid);
	
	/*Define new RGB type to deliver*/
	MPI_Datatype rgbtype;
	MPI_Type_contiguous(3,MPI_BYTE,&rgbtype);
	MPI_Type_commit(&rgbtype);
	
	//讀取檔案
	//if(myid==0)
	//{
		if ( readBMP( infileName) )
		{
			if(myid==0)
				cout << "Read file successfully!!" << endl;		
		}
		else
		{			
			cout << "Read file fails!!" << endl;
		}	
	//}
	
	/*Local RGB data*/
	RGBTRIPLE **myBMPSaveData = NULL;                                               
	RGBTRIPLE **myBMPData = NULL;
	myBMPData = alloc_memory( (bmpInfo.biHeight/nproc)+2, bmpInfo.biWidth);
	myBMPSaveData = alloc_memory( (bmpInfo.biHeight/nproc)+2, bmpInfo.biWidth);
	int recvcnt = (bmpInfo.biHeight/nproc) * bmpInfo.biWidth; //Every proc receive "revcnt" rgbdata
	int sendcnt[nproc];
	int disp[nproc];
	
	for(int i=0;i < nproc;++i){
		sendcnt[i] = recvcnt;//send data count = recv data count
		disp[i] = i*recvcnt;//disp = i*recvcnt since that all sendcounts are the same
	}
	
	//Scatter rgbdata
	MPI_Scatterv(BMPSaveData[0],sendcnt, disp,rgbtype, myBMPSaveData[1], recvcnt, rgbtype, 0, MPI_COMM_WORLD);

	//define left proc & right proc
	int lproc,rproc;
	lproc=myid-1;
	rproc=myid+1;
	MPI_Status istat[8];
	if(myid==0)
	{
		lproc=nproc-1;
	}	
	if(myid==nproc-1)
	{
		rproc=0;
	}
	
	MPI_Barrier(MPI_COMM_WORLD);
	//start time
	startwtime = MPI_Wtime();
    //進行多次的平滑運算
	for(int count = 0; count < NSmooth ; count ++){
		//把像素資料與暫存指標做交換
		swap(myBMPSaveData,myBMPData);
		
		MPI_Sendrecv((void *)&myBMPData[(bmpInfo.biHeight/nproc)][0], bmpInfo.biWidth,rgbtype, rproc, 110,(void *)&myBMPData[0][0], bmpInfo.biWidth, rgbtype, lproc, 110, MPI_COMM_WORLD, istat);
		MPI_Sendrecv((void *)&myBMPData[1][0], bmpInfo.biWidth,rgbtype, lproc, 120,(void *)&myBMPData[(bmpInfo.biHeight/nproc)+1][0], bmpInfo.biWidth, rgbtype, rproc, 120, MPI_COMM_WORLD, istat);
		
		//進行平滑運算
		for(int i = 1; i< (bmpInfo.biHeight/nproc)+1; i++)
			for(int j =0; j<bmpInfo.biWidth ; j++){
				/*********************************************************/
				/*設定上下左右像素的位置                                 */
				/*********************************************************/
				int Top = i>0 ? i-1 : bmpInfo.biHeight-1;//i always >0 
				int Down = i<(bmpInfo.biHeight/nproc)+1 ? i+1 : 0; // i always <(bmpInfo.biHeight/nproc)+1
				int Left = j>0 ? j-1 : bmpInfo.biWidth-1;
				int Right = j<bmpInfo.biWidth-1 ? j+1 : 0;
				/*********************************************************/
				/*與上下左右像素做平均，並四捨五入                       */
				/*********************************************************/
				myBMPSaveData[i][j].rgbBlue =  (double) (myBMPData[i][j].rgbBlue+myBMPData[Top][j].rgbBlue+myBMPData[Down][j].rgbBlue+myBMPData[i][Left].rgbBlue+myBMPData[i][Right].rgbBlue)/5+0.5;
				myBMPSaveData[i][j].rgbGreen =  (double) (myBMPData[i][j].rgbGreen+myBMPData[Top][j].rgbGreen+myBMPData[Down][j].rgbGreen+myBMPData[i][Left].rgbGreen+myBMPData[i][Right].rgbGreen)/5+0.5;
				myBMPSaveData[i][j].rgbRed =  (double) (myBMPData[i][j].rgbRed+myBMPData[Top][j].rgbRed+myBMPData[Down][j].rgbRed+myBMPData[i][Left].rgbRed+myBMPData[i][Right].rgbRed)/5+0.5;
			}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	//得到結束時間，並印出執行時間
	endwtime = MPI_Wtime();
	if(myid==0)
		cout << "The execution time = "<< endwtime-startwtime <<endl ;
	//gather saveData
	MPI_Gatherv(myBMPSaveData[1],recvcnt,rgbtype,BMPSaveData[0],sendcnt,disp,rgbtype,0,MPI_COMM_WORLD);
 
 	//寫入檔案
	if(myid==0)
	{
        if ( saveBMP( outfileName1 ) )
                cout << "Save file successfully!!" << endl;
        else
                cout << "Save file fails!!" << endl;
	}
 	free(BMPData);
 	free(BMPSaveData);
	free(myBMPData);
	free(myBMPSaveData);
 	MPI_Finalize();

        return 0;
}

/*********************************************************/
/* 讀取圖檔                                              */
/*********************************************************/
int readBMP(char *fileName)
{
	//建立輸入檔案物件	
        ifstream bmpFile( fileName, ios::in | ios::binary );
 
        //檔案無法開啟
        if ( !bmpFile ){
                cout << "It can't open file!!" << endl;
                return 0;
        }
 
        //讀取BMP圖檔的標頭資料
    	bmpFile.read( ( char* ) &bmpHeader, sizeof( BMPHEADER ) );
 
        //判決是否為BMP圖檔
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }
 
        //讀取BMP的資訊
        bmpFile.read( ( char* ) &bmpInfo, sizeof( BMPINFO ) );
        
        //判斷位元深度是否為24 bits
        if ( bmpInfo.biBitCount != 24 ){
                cout << "The file is not 24 bits!!" << endl;
                return 0;
        }

        //修正圖片的寬度為4的倍數
        while( bmpInfo.biWidth % 4 != 0 )
        	bmpInfo.biWidth++;

        //動態分配記憶體
        BMPSaveData = alloc_memory( bmpInfo.biHeight, bmpInfo.biWidth);
        
        //讀取像素資料
    	//for(int i = 0; i < bmpInfo.biHeight; i++)
        //	bmpFile.read( (char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE));
	bmpFile.read( (char* )BMPSaveData[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight);
	
        //關閉檔案
        bmpFile.close();
 
        return 1;
 
}
/*********************************************************/
/* 儲存圖檔                                              */
/*********************************************************/
int saveBMP( char *fileName)
{
 	//判決是否為BMP圖檔
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }
        
 	//建立輸出檔案物件
        ofstream newFile( fileName,  ios:: out | ios::binary );
 
        //檔案無法建立
        if ( !newFile ){
                cout << "The File can't create!!" << endl;
                return 0;
        }
 	
        //寫入BMP圖檔的標頭資料
        newFile.write( ( char* )&bmpHeader, sizeof( BMPHEADER ) );

	//寫入BMP的資訊
        newFile.write( ( char* )&bmpInfo, sizeof( BMPINFO ) );

        //寫入像素資料
        //for( int i = 0; i < bmpInfo.biHeight; i++ )
        //        newFile.write( ( char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE) );
        newFile.write( ( char* )BMPSaveData[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight );

        //寫入檔案
        newFile.close();
 
        return 1;
 
}


/*********************************************************/
/* 分配記憶體：回傳為Y*X的矩陣                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X )
{        
	//建立長度為Y的指標陣列
        RGBTRIPLE **temp = new RGBTRIPLE *[ Y ];
	RGBTRIPLE *temp2 = new RGBTRIPLE [ Y * X ];
        memset( temp, 0, sizeof( RGBTRIPLE ) * Y);
        memset( temp2, 0, sizeof( RGBTRIPLE ) * Y * X );

	//對每個指標陣列裡的指標宣告一個長度為X的陣列 
        for( int i = 0; i < Y; i++){
                temp[ i ] = &temp2[i*X];
        }
 
        return temp;
 
}
/*********************************************************/
/* 交換二個指標                                          */
/*********************************************************/
void swap(RGBTRIPLE *a, RGBTRIPLE *b)
{
	RGBTRIPLE *temp;
	temp = a;
	a = b;
	b = temp;
}

