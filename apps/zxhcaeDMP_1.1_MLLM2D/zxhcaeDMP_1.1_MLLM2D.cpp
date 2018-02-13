

 /*=========================================================================

  Program:   ZXH CoronaryArteryExtraction Software
  Author:	 Dengqiang Jia
  Module:    $RCSfle: zxhcaeDMP.cpp    $
  Language:  C++
  Date:      $Date: From  2011-03 $
  Version:   $Revision: 2.2.1 $

=========================================================================*/
/// \brief
/// Spatially encoded mutual information + free-form deformation registration
/// Linear
/// gradient descent
/// save all registration info into zxhRegistrationStruct StructRegistration which set to optimiser
/// For concatenated transforms all (for FFD and LocallyAffines in optimization regularisation step)
///                                 First update zxhRegistrationStruct::m_pConcatenatedTransformsByRegridding(spacing 1mm)
///                                 then transform all images in ref_space using zxhRegistrationStruct.m_CopyRefXXXOrg
///                                 finally set current transform to identity
/// For preset transformation, unless same spacing FFD for the first Reg, otherwise would be treated as concatenation
///
#include<iostream>
using namespace std;
void Help()
{
	std::cout<<" An simple example for extraction centerlines  of CTA images, target.nii.gz and source.nii.gz, result save as res: \n" ;
	std::cout<<" zxhsemi0 -test target.nii.gz -ref source.nii.gz -o result0 -ffd 20 20 20 -bending 0.001\n";
	std::cout<<" zxhsemi -test target.nii.gz -ref source.nii.gz -o result -pre 0 result0.FFD -ffd 20 20 20 -ffd 10 10 10 -Reg 2 -sub 2 2 2 -sub 1 1 1 -bending 0.001\n";
	std::cout<<"OR \n";
	std::cout<<" zxhsemi -test target.nii.gz -ref source.nii.gz -o result -hierarchy 3 -bending 0.0031\n\n";

	std::cout<<"  <-test/target img.>     (test or target image)\n" ;
	std::cout<<"  <-ref/source img.>      (reference or source image)\n" ;
	std::cout<<"  <-o savename>           (string for saving transformed -ref/-source image, file names prefix)\n" ;
	std::cout<<"  <-maskt img.>           (mask image on test image, use -maskr on ref image) \n" ;
	std::cout<<"  USE -ffd: zxhsemi0 to fast init and get the .FFD for setting -pre, and then set -ffd\n" ;
	std::cout<<"  <-ffd fx fy fz>         (spacing of free form deformation, FFD, multi-input related to -Reg)\n" ;
	std::cout<<"  <-pre 0 s>              (pre set transformation field)\n";
	std::cout<<"  <-sub fx fy fz [ft]>    ([3 3 3], sampling spacing; positive: millimeters interval; negative: pixels interval)\n";
	std::cout<<"  <-Reg i>                ([1] number of multi-level registrations)\n" ;
	std::cout<<"  OR USE -hierarchy, simple and not need to set -ffd,-sub,-Reg:\n" ; 
	std::cout<<"  <-hierarchy n>          ([3] number of multi-level FFD registration, where\n";
	std::cout<<"                           the first level of -ffd spacing is one forth of test image extent, and halve in subsequence level\n" ; 
	std::cout<<"                           the final level of -sub sampling is pixel size of the test image\n" ; 	 
	std::cout<<"\n";
	std::cout<<"  <-bending f..f>         ([0.001]weighting for bending energy, recommend f=0.001~0.01)\n" ;
} 
void HELP()
{ 
	Help() ;
	std::cout<<"------------------------------------------------------\n" ;
	std::cout<<"  OPTIONS of gradient optimization computation; use setting in previous -Reg when un-set \n" ; 
}  





#include "miiMinPathModel.h"

#include "vtkUnstructuredGrid.h"
#include "vtkSmartPointer.h"
#include "vtkUnstructuredGridReader.h"
#include "vtkUnstructuredGridWriter.h"
#include "vtkPolyLine.h"

#include <time.h>

#define TAB_CHAR	9
#define M_PI 3.14159265358979323846

int GetTime() ;


typedef struct
{
	double x;
	double y;
	double z;
}PointCordTypeDef;

bool ReadMeanStdDevFromTxt(char *chFileName, double &fMean, double &fStdDev)
{
	string strNum;
	int nStart = 0, nEnd = 0;

	ifstream iFileIn(chFileName);
	if(!iFileIn)
	{
		cout << "Unable to open txt-file: " << endl;
		cout << chFileName << endl;
		return false; //exit(1); // terminate with error
	}

	string strLine;
	if (!getline(iFileIn, strLine))
	{
		cout << "The format of Mean and StdDev file is wrong!" << endl;
		return false;
	}

	// read 1st number
	nEnd = strLine.find_first_of(TAB_CHAR, nStart);
	strNum.assign(strLine, nStart, nEnd - nStart);
	fMean = atof(strNum.c_str());
	//fMean=1024*fMean/1896;//Add by JDQ
	// read 2nd number
	nStart = nEnd + 1;
	nEnd = strLine.find_first_of(TAB_CHAR, nStart);
	strNum.assign(strLine, nStart, nEnd - nStart);
	fStdDev = atof(strNum.c_str());
	//fStdDev=1024*fStdDev/1896;//Add by JDQ

	return true;
}
bool ReadVtk(char *chFileName, vector< miiCNode<double, float> > &vPointCord)
{
	if (chFileName == NULL)
	{
		cerr << "Cannot find VTK file!" << endl;
		return false;
	}
	//	char* chFileName = "reference.vtk";
	vtkSmartPointer<vtkUnstructuredGridReader> iVtkReader = vtkSmartPointer<vtkUnstructuredGridReader>::New();
	iVtkReader->SetFileName( chFileName );
	iVtkReader->Update();

	vtkSmartPointer<vtkUnstructuredGrid> iVtkGridRead = iVtkReader->GetOutput();

	int nPointNum = iVtkGridRead->GetMaxCellSize();

	double fCord[3];
	miiCNode<double, float> strctTempPoint;

	for (int i = 0; i < nPointNum; i++)
	{
		iVtkGridRead->GetPoint(i, fCord);

		strctTempPoint.x = fCord[0];
		strctTempPoint.y = fCord[1];
		strctTempPoint.z = fCord[2];
		vPointCord.push_back(strctTempPoint);
	}

	

	return true;
}

bool ReadPointTxt(char *filename,vector< miiCNode<double, float> > &cl)
{
    string strNum;
	int nStart = 0, nEnd = 0;
	miiCNode<double, float> strctTempPoint;
	ifstream iFileIn(filename);
	if(!iFileIn)
	{
		cout << "Unable to open txt-file: " << endl;
		cout << filename << endl;
		return false; //exit(1); // terminate with error
	}
	string strLine;
	if (!getline(iFileIn, strLine))
	{
		cout << "The format of Mean and StdDev file is wrong!" << endl;
		return false;
	}
	// read 1st number
	nEnd = strLine.find_first_of(TAB_CHAR, nStart);
	strNum.assign(strLine, nStart, nEnd - nStart);
	strctTempPoint.x =atof(strNum.c_str());
	//fMean=1024*fMean/1896;//Add by JDQ
	// read 2nd number
	nStart = nEnd + 1;
	nEnd = strLine.find_first_of(TAB_CHAR, nStart);
	strNum.assign(strLine, nStart, nEnd - nStart);
	strctTempPoint.y =atof(strNum.c_str());
	// read 3rd number
	nStart = nEnd + 1;
	nEnd = strLine.find_first_of(TAB_CHAR, nStart);
	strNum.assign(strLine, nStart, nEnd - nStart);
	strctTempPoint.z =atof(strNum.c_str());
	cl.push_back(strctTempPoint);
    return true;
}
bool ReadandSetMolMeanFromTxt(char *chModelMapCAFileName,char *chFileName,vector< miiCNode<double, float> > &vModelPointsWithIntensity)
{
	if(vModelPointsWithIntensity.size()>0)vModelPointsWithIntensity.clear ();
	vector<double>vModelIntensity;
	ifstream inMolIntdata(chFileName);
	double data;
	string strLine="";
	bool bsizeeque=true;
	
	if(inMolIntdata)
	{

		while(getline(inMolIntdata,strLine,'\n'))
		{ 

			data = atof(strLine.c_str());
			vModelIntensity.push_back(data);

		}
	}
	else
	{
		cout << "Unable to open txt-file: " << endl;
		cout << chFileName << endl;
		return false; //exit(1); // terminate with error
	}

	ReadVtk(chModelMapCAFileName, vModelPointsWithIntensity);

	if(vModelPointsWithIntensity.size()==vModelIntensity.size())
	{

		for (int i=0;i<vModelPointsWithIntensity.size();i++)
		{
			vModelPointsWithIntensity[i].val=vModelIntensity[i];
		}
	}
	else
	{
		bsizeeque=false;

	}
	inMolIntdata.close();
	return bsizeeque;
}


void WriteVtk(vector< miiCNode<double, float> > vPointCord, char *chFileName)
{
	vtkSmartPointer<vtkPoints> iPoints = vtkSmartPointer<vtkPoints>::New();

/*	int nPointNum = PointCord.size();*/

	float fImgPixel[3];
	int nPointNum = vPointCord.size();

	for (int i = 0; i < nPointNum; i++)
	{
		iPoints->InsertNextPoint(vPointCord[i].x, vPointCord[i].y, vPointCord[i].z);
	}	

	vtkSmartPointer<vtkPolyLine> iLine = vtkSmartPointer<vtkPolyLine>::New();
	iLine->GetPointIds()->SetNumberOfIds(nPointNum);
	for (int i = 0; i < nPointNum; i++)
	{
		iLine->GetPointIds()->SetId(i, i);
	}

	vtkSmartPointer<vtkUnstructuredGrid> iGrid = vtkUnstructuredGrid::New();
	iGrid->Allocate(1, 1);	
	iGrid->InsertNextCell(iLine->GetCellType(), iLine->GetPointIds());
	iGrid->SetPoints(iPoints);

	vtkSmartPointer<vtkUnstructuredGridWriter> iVtkWriter = vtkUnstructuredGridWriter::New();
	iVtkWriter->SetInput(iGrid);
	iVtkWriter->SetFileName(chFileName);
	iVtkWriter->Write();
}

	void WriteModCA2Txt(char *chFileName,const vector<double>m_sMolPontIntsFinl)
	{
	ofstream WriteFileTxt(chFileName,ios::out);
	int nPointNum = m_sMolPontIntsFinl.size();
	float fmodints;	
	for (int i = 0; i < nPointNum; i++)
	{
	 fmodints= m_sMolPontIntsFinl[i];
     WriteFileTxt <<right<<fixed<<setfill('0')<<setprecision(4)<< fmodints<< '\n';

	}	
	
	}
	/*void WriteCA2Txt(vector< miiCNode<double, float> > vPointCord, char *chFileName)
	{
	ofstream WriteFileTxt(chFileName);
	int nPointNum = vPointCord.size();

	for (int i = 0; i < nPointNum; i++)
	{
		
     WriteFileTxt << vPointCord[i].x << " " << vPointCord[i].y << " " << vPointCord[i].z << "/n ";

	}	
	
	}
	*/
	

void CalcUnseenMeanStdDev(double gAortaMean[2], double gAortaStdDev[2], double fAtlsCAMean, double fAtlsCAStdDev,
	double &fUnseenCAMean, double &fUnseenCAStdDev)
{
	double fAtlsAortaMean = gAortaMean[0];
	double fUnseenAortaMean = gAortaMean[1];
	

	double fMeanDiff = fAtlsAortaMean - fAtlsCAMean;
	fUnseenCAMean = fUnseenAortaMean - fMeanDiff;

	double fAtlsAortaStdDev = gAortaStdDev[0];
	double fUnseenAortaStdDev = gAortaStdDev[1];

	fUnseenCAStdDev = fAtlsCAStdDev * fUnseenAortaStdDev / (fAtlsAortaStdDev + 0.00000001);	
}
bool ModifiedCalcUnseenMeanStdDev(double gAortaMean[2],double gAortaStdDev[2],double fAtlsCAMean,double &fUnseenCAMean,double &fUnseenCAStdDev)//calculate the first mean std of unseen image.
{
	fUnseenCAMean=gAortaMean[1];//fAtlsCAMean-gAortaMean[0]+gAortaMean[1];
	fUnseenCAStdDev=gAortaStdDev[1];
	return true;
}

bool CalcAtlsCAMeanStdDev(string strFileNameAtls, char *strFileNameCA, double &fAtlsCAMean, double &fAtlsCAStdDev)
{
	//read a "nifti" file
	zxhImageDataT<short> imgReadAtls; 
	if( zxh::OpenImage( &imgReadAtls, strFileNameAtls ) == false )
	{
		std::cerr << " Raw image(nifti-file) is not found!"; 
		return false;
	}

	vector< miiCNode<double, float> > vAtlsCAPoints;
	ReadVtk(strFileNameCA, vAtlsCAPoints);

	// get the image size
	int nImgWX, nImgWY, nImgWZ, nImgWT;
	imgReadAtls.GetImageSize(nImgWX, nImgWY, nImgWZ, nImgWT);

	// get the image data
	const short *sImgData = imgReadAtls.GetImageData();

	int nSearchRange = 8;
	int nCount = 0;
	double nSum = 0;
	float fCord[3];
	fCord[0] = vAtlsCAPoints[10].x;
	fCord[1] = vAtlsCAPoints[10].y;
	fCord[2] = vAtlsCAPoints[10].z;
	imgReadAtls.GetImageInfo()->WorldToImage(fCord);
	int nXmin = (int)(fCord[0]+0.5) - nSearchRange;
	int nXmax = (int)(fCord[0]+0.5) + nSearchRange;

	int nYmin = (int)(fCord[1]+0.5) - nSearchRange;
	int nYmax = (int)(fCord[1]+0.5) + nSearchRange;

	int nZmin = (int)(fCord[2]+0.5) - nSearchRange;
	int nZmax = (int)(fCord[2]+0.5) + nSearchRange;
	
	// calculate mean
	for (int iz = nZmin; iz < nZmax; iz++)
	{
		for (int iy = nYmin; iy < nYmax; iy++)
		{
			for (int ix = nXmin; ix < nXmax; ix++)
			{
				if (sImgData[iz * nImgWY * nImgWX + iy * nImgWX + ix] > 10)
				{
					nCount++;
					nSum += (double)sImgData[iz * nImgWY * nImgWX + iy * nImgWX + ix];
				}
			}
		}
	}

	fAtlsCAMean = nSum / (nCount + 0.00000000001);
	nSum = 0;

	// calculate deviation
	for (int iz = nZmin; iz < nZmax; iz++)
	{
		for (int iy = nYmin; iy < nYmax; iy++)
		{
			for (int ix = nXmin; ix < nXmax; ix++)
			{
				if (sImgData[iz * nImgWY * nImgWX + iy * nImgWX + ix] > 10)
				{
					double temp = sImgData[iz * nImgWY * nImgWX + iy * nImgWX + ix] - fAtlsCAMean;
					temp *= temp;
					nSum += temp;
				}
			}
		}
	}

	fAtlsCAStdDev = sqrt(nSum / (nCount + 1));

	imgReadAtls.ReleaseMem();

	return true;
}
bool ModifiedCalcAtlsCAMeanStdDev(vector< miiCNode<double, float> > vModelPointsWithIntensity, double &fAtlsCAMean)
{
	//fAtlsCAMean=vModelPointsWithIntensity[0].val;
	float inten=0;
	for(int i=0;i<vModelPointsWithIntensity.size();i++)
	{
		inten=inten+vModelPointsWithIntensity[i].val;

	}

	fAtlsCAMean=inten/(vModelPointsWithIntensity.size());
	return true;
}
bool SetMolPointsIntensity(char *chModelMapCAFileName,string strFileNameAtls,vector< miiCNode<double, float> > &vModelPointsWithIntensity)
{
	
	zxhImageDataT<short> imgReadAtls; 
	if( zxh::OpenImage( &imgReadAtls, strFileNameAtls ) == false )
	{
		std::cerr << " Raw image(nifti-file) is not found!"; 
		return false;
	}

	
	ReadVtk(chModelMapCAFileName, vModelPointsWithIntensity);

	// get the image size
	int nImgWX, nImgWY, nImgWZ, nImgWT;
	imgReadAtls.GetImageSize(nImgWX, nImgWY, nImgWZ, nImgWT);

	// get the image data
	const short *sImgData = imgReadAtls.GetImageData();

	int nSearchRange = 8;
	for (int i=0;i<vModelPointsWithIntensity.size();i++)
	{
	int nCount = 0;
	double nSum = 0;
	float fCord[3];
	fCord[0] =vModelPointsWithIntensity[i].x;
	fCord[1] =vModelPointsWithIntensity[i].y;
	fCord[2] =vModelPointsWithIntensity[i].z;
	imgReadAtls.GetImageInfo()->WorldToImage(fCord);
	int nXmin = (int)(fCord[0]+0.5) - nSearchRange;
	int nXmax = (int)(fCord[0]+0.5) + nSearchRange;
	
	int nYmin = (int)(fCord[1]+0.5) - nSearchRange;
	int nYmax = (int)(fCord[1]+0.5) + nSearchRange;

	int nZmin = (int)(fCord[2]+0.5) - nSearchRange;
	int nZmax = (int)(fCord[2]+0.5) + nSearchRange;
	if( nXmin<0)nXmin=0;
	if( nXmax>nImgWX)nXmin=nImgWX;
	if( nYmin<0)nYmin=0;
	if( nYmax>nImgWY)nYmin=nImgWY;
	if( nZmin<0)nZmin=0;
	if( nZmax>nImgWZ)nZmin=nImgWZ;

	// calculate mean
	for (int iz = nZmin; iz < nZmax; iz++)
	{
		for (int iy = nYmin; iy < nYmax; iy++)
		{
			for (int ix = nXmin; ix < nXmax; ix++)
			{
				if (sImgData[iz * nImgWY * nImgWX + iy * nImgWX + ix] > 10)
				{
					nCount++;
					nSum += (double)sImgData[iz * nImgWY * nImgWX + iy * nImgWX + ix];
				}
			}
		}
	}

	vModelPointsWithIntensity[i].val= nSum / (nCount + 0.00000000001);
	nSum = 0;
		
	}
	imgReadAtls.ReleaseMem();

	return true;
}
void RandomDefPoint(float RefStart[3])
{
	int RAD_2=2;
		float fFi=0,fTheta=0,R=0;
		srand((unsigned)time(NULL));
		fFi=(float)M_PI* (rand()%360);
		float fRandVector[3]={0,0,0};
			fTheta=(float)M_PI*(rand()%180);
	R=(float)RAD_2*(rand() % 20)/19.0;
	fRandVector[0]=R*sin(fTheta)*cos(fFi);
	fRandVector[1]=R*sin(fTheta)*sin(fFi);
	fRandVector[2]=R*cos(fTheta);
	RefStart[0]=RefStart[0]+fRandVector[0];
	RefStart[1]=RefStart[1]+fRandVector[1];
	RefStart[2]=RefStart[2]+fRandVector[2];
}

int main(int argc, char *argv[])//Use Resolution Mask to extraction
{
	
	
//for script file***
int o=0;



string strFileNameRaw ="F:/2D_MP/single0.nii.gz";
//string strFileNameVsls= "F:/Coronary_0/Coronary_Niessen/ProcessByJDQ/training/dataset07/Remol_image27_vesselness.nii";//  string(argv[2]);
//char *chModelMapCAFileName= "F:/Coronary_0/Coronary_Niessen/mean_centerline_2014_01_17/image07_meancenterline_v1.vtk";// argv[3];
//char *chResultPath= "F:/Coronary_0/testingdataZXHCAEDMP/LowResoResults/mod07_to_unseen27_results/meanimg07v1model";// argv[4];
//char *chAtlasCAFileName = "F:/Coronary_0/Coronary_Niessen/mean_centerline_2014_01_17/image07_meancenterline_v1.vtk";// argv[5];
//char *chREFFileName =  "F:/Coronary_0/Exp6_detect_ostia/Ostia0007To0831/Ostia0007To0831/dataset27_Meanostium_left.txt";// argv[6];
//char *chAtlsMeanStdDevFileName ="F:/Coronary_0/meanstd/atlas07_mean_std.txt "; //  argv[7];
//char *chUnseenMeanStdDevFileName ="F:/Coronary_0/meanstd/atlas27_mean_std.txt";// argv[8];
//char *chMolPointsIntensityFileName="F:/Coronary_0/Coronary_Niessen/mean_centerline_intensity/image07_07mline_v1_inten.txt";//argv[9];
int nMaxSgmtNum =30;//atoi(argv[10]);
	
float fRefStartPointWorld[3]={-103.8,-57.3,0};

miiCNode<double, float> NRefStartWorld;
NRefStartWorld.x=fRefStartPointWorld[0];
NRefStartWorld.y=fRefStartPointWorld[1];
NRefStartWorld.z=fRefStartPointWorld[2];
NRefStartWorld.val=0;



//read a "nifti" file
zxhImageDataT<short> imgReadRaw; //Change by JDQ
if( zxh::OpenImage( &imgReadRaw, strFileNameRaw ) == false )
{
	std::cerr << "Raw image(nifti-file) is not found!"; 
	return -1;
}
imgReadRaw.GetImageInfo()->WorldToImage(fRefStartPointWorld);
miiCNode<double, short> NRefStart;
NRefStart.x=(short) (fRefStartPointWorld[0]+0.5);
NRefStart.y=(short) (fRefStartPointWorld[1]+0.5);
NRefStart.z=(short) (fRefStartPointWorld[2]+0.5);
NRefStart.val=0;
 
		//get the number of none-zero points
		
		
}


int GetTime()
{
	time_t now_time;
	now_time = time(NULL);
	return now_time;
}


 