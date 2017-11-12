#include <stdio.h>     // printf()
#include <limits.h>    // UINT_MAX
#include <stdlib.h>
#include "mpi.h"
#include <math.h>
#include <time.h>

//every process generate size rand number under 10000 
void generateRand(int mykey[],int size)
{
	int i;
	srand(getpid());
	for(i=0;i<size;++i){
		mykey[i] = (rand()%10000);
	}
	return;
}

//process to get it partner 
int Compute_partner(int phase,int id,int numprocs)
{
	int partner;
	if(phase%2==0)//even phase
	{
		if(id%2!=0)
		{
			partner=id-1;	
		}
		else
		{
			partner=id+1;
		}
	}
	
	else //odd phase
	{
		if(id%2!=0)
		{
			partner=id+1;	
		}
		else
		{
			partner=id-1;
		}
	}
	
	//partner out of range
	if(partner == -1 || partner == numprocs) //is head or tail process and don't have partner during odd phase
	{
		partner = MPI_PROC_NULL;
	}

	return partner;
}

//Use serial odd-even sort on every process local sort
void localSort(int mykey[],int size)
{
	int i,phase,tmp;
	for(phase=0;phase<size;++phase)
	{
		if(phase%2==0)
		{
			for(i=1;i<size;i+=2)
			{
				if(mykey[i-1]>mykey[i])
				{
					tmp = mykey[i];
					mykey[i] = mykey[i-1];
					mykey[i-1]=tmp;
				}
			}
		}
		else
		{
			for(i=1;i<size-1;i+=2)
			{
				if(mykey[i] > mykey[i+1])
				{
					tmp = mykey[i];
					mykey[i] = mykey[i+1];
					mykey[i+1]=tmp;
				}
			}
		}
	}
	return;
}

//change the small numbers to small id proc & big ones to big id proc(use odd-even sort on ppt)
void keySort(int mykey[],int ourkeys[],int size,int id,int keys[],int n,int numprocs)
{
	int phase,i,partner;
	for(phase=0;phase<numprocs;++phase)
	{
		partner=Compute_partner(phase,id,numprocs);
		
		if(partner!=MPI_PROC_NULL)
		{	
			//partner communication
			MPI_Status status;
			MPI_Send(&mykey[0],size, MPI_INT,partner,1, MPI_COMM_WORLD);  //send value to partner node
			MPI_Recv(&ourkeys[0],size, MPI_INT, partner, 1,MPI_COMM_WORLD, &status);  //receive value from partner
		
		for(i=0;i < size ;++i)
			{
				ourkeys[size+i]=mykey[i];//append local key to ourkeys
			}
			
			localSort(ourkeys,size*2);//sort ourkeys
			
			if(id<partner) // if proc is smaller than its partner, get the 1~size number in ourkeys(after sort)
			{
				for(i=0;i < size ;++i)
				{
					mykey[i]=ourkeys[i];
				}
			}
	
			else//if proc is bigger than its partner, get the size+1~size*2 number in ourkeys(after sort)
			{
				for(i=0;i < size ;++i)
				{
					mykey[i]=ourkeys[size+i];
				}
			}
		}
	
		//gather every processes' local key to keys
		MPI_Gather((void*)&mykey[0],size,MPI_INT,(void*)&keys[0],size,MPI_INT,0,MPI_COMM_WORLD);
		if(id==0)
		{
			//then if rank=0 print the gathered keys every phase
			printf("phase %2d :",phase);
			for(i =0;i<n;++i)
			{
				printf("%d\t",keys[i]);
			}
			printf("\n");
		}
	}
	
}

int main(int argc, char *argv[]){
	
	/*MPI declaration*/
	int numprocs=0,id=0;
	
	/*************************************
	  
	 ***********initialize MPI**********
	   
	*************************************/
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD,&id);
	
	
	/*Get the keys number n*/
	int n;
	if(id==0)
	{
		printf("Give me the keys you want to generate:");
		scanf(" %d",&n);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Bcast(&n,1,MPI_INT,0,MPI_COMM_WORLD);//broadcast keys number to all process
	
	//declaration
	int size = n/numprocs;
	int mykey[size],ourkeys[size*2],keys[n];
	
	//execute needed function
	generateRand(mykey,size);	
	localSort(mykey,size);
	keySort(mykey,ourkeys,size,id,keys,n,numprocs);
	
	//finalize MPI
	MPI_Finalize();
	
	return 0;
	
}