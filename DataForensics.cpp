//============================================================================
// Name        : DataForensics.cpp
// Author      : Dongao Li, Hang Liu
// Time		   : 23/2/2017
//============================================================================

#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <string.h>
#include <iomanip>
using namespace std;
struct Partition{ string type; int start; int size;} parts[4];
// the struct of partition that is used to store the information of every partition
ifstream file;

/**
 * @param: char File_path[]
 *
 * @brief: open the Imager file
 *
 * @return: void
 */
void readFile(char File_path[]){
	file.open(File_path,ios::in | ios::binary);
		if (!file.is_open()){
			cout<< "Error opening file!!!"<<endl;
			exit(1);
		}
}

/**
 * @param: char array[], int start, int num
 *
 * @brief: the array[] is the objective array, the start is the location of element that need to do
 *       shift operation in the array, the num is the number of element that need be operated.
 *       According those three parameters to get the elements and do the left shift operation.
 *
 * @return: offset
 */
int getOffset(char array[], int start, int num){
	int offset;

	if (num==2){
		offset = ((int)(unsigned char)(array[start+1])<<8) | ((int)(unsigned char)array[start] & 0x00ff);
	}
	else if (num==4){
		offset=((int)(unsigned char)array[start+1]<< 8 );
		offset= offset | ((int)(unsigned char)array[start] & 0x000000ff);
		offset= (offset & 0x0000ffff) | ((int)(unsigned char)array[start+3] << 24 ) | ((int)(unsigned char)array[start+2] << 16 ) ;
	}
	return offset;
}

/**
 * @param: void
 *
 * @brief: read the MBR partition, and put the every partitions information into parts[4] struct
 *       final to print all of the information
 *
 * @return: void
 */
void readMBR(){
	char buf[64]; // read 64 bytes data
	int validParts=4;
	file.seekg(0x1BE, ios::beg);
	file.read(buf,64);
	cout<<"~~~~~~~~~~~~~~~~~~~~The MBR information~~~~~~~~~~~~~~~~~~~"<<endl;
	for (int i=0; i<4; i++){
		parts[i].start=*(int*)(buf + 0x08 +(i * 16));
		parts[i].size=*(int*)(buf + 0x0C + (i * 16));
		switch (buf[ 0x04 +(i * 16)]) {
			case 0x00 :
				parts[i].type="NOT-VALID";
				validParts--;
				break;
			case 0x06 :
				parts[i].type="FAT-16";
				break;
			case 0x07 :
				parts[i].type= "NTFS";
				break;
			case 0x0B:
				parts[i].type="FAT-32";
				break;
			default:
				parts[i].type="NOT-DECODED";
				break;
			}
		cout<<"Partition: "<<i<<"  Type: "<<parts[i].type<<"  Start: "<<parts[i].start <<"  Size: "<<parts[i].size<<endl;
	}
	cout<<endl<<"Total number of valid partitions is: "<<validParts<<endl;


}

/**
 * @param: start - the entry of FAT16
 *
 * @brief: read the FAT-16 partition with the start offset, and print all of information
 *
 * @return: void
 */
void readFAT16(int start){
	char FAT16[64], delete_file[32], delete_file_content[16];
	char find_delete_file[2]; // read the first byte of every directory entry to find the delete file
	int delete_file_size;
	int start_cluster;
	int CSA;
	file.seekg(start);
	file.read(FAT16,64);
	int num_sector = (int)(unsigned char)(FAT16[0x0D]);
	int size_FAT = (int)(FAT16[0x10]) * getOffset(FAT16, 0x16, 2);
	int size_reserved = getOffset(FAT16, 0x0E, 2);
	//cout<<"int size_reserved:    "<<size_reserved<<endl;
	int size_root_dir = getOffset(FAT16, 0x11, 2) * 32 / 512;
	int add_clu2= 63 + size_reserved + size_FAT +size_root_dir;

	cout<<"\n~~~~~~~~~~~~~~~~~~~~FAT-16 information~~~~~~~~~~~~~~~~~~~~"<<endl;

	cout<<"+++Basic info+++"<<endl;
	cout<<"number of sectors per cluster: "<<num_sector<<endl;
	cout<<"The size of the FAT area: "<<size_FAT<<endl;
	cout<<"The size of the Root directory: "<<size_root_dir<<endl;
	cout<<"The sector address of Cluster #2: "<<add_clu2<<endl;

	cout<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
	cout<<"The information of first deleted file on the volume's root directory:"<<endl;
	//root directory address is the data area start address, so it = (first sector of volume) + (size of reserved area) + (size of FAT Area)
	int root_dir= (size_FAT + size_reserved + 63 )* 512;
	for (int i=root_dir; i<add_clu2*512; i+=0x20){
		file.seekg(i);
		file.read(find_delete_file,1);
		if ((int)(unsigned char)find_delete_file[0]==229){//the first byte of deleted file is 0xE5
			file.seekg(i);
			file.read(delete_file,32);
			break;
		}
	}
	if ((int)(unsigned char)delete_file[0]!=229){
		cout<<"No deleted file on the volume's root directory!"<<endl;
		return;
	}
	cout<<"The name of the file: ";
	for (int i=0; i<11; i++){
		cout<<(delete_file[i]);
	}
	cout<<endl;
	delete_file_size = getOffset(delete_file, 28, 4);
	cout<<"the size of the file: "<<delete_file_size<<" ("<<hex<<delete_file_size<<") "<<endl;
	start_cluster = getOffset(delete_file, 26, 2);
	cout<<"the number of the first cluster: "<<dec<<start_cluster<<endl;
	CSA=add_clu2 + (start_cluster - 2) * num_sector;
	cout<<"The address of the cluster sector: "<<CSA<<endl;
	cout<<"The first 16 characters of the content of this file:"<<endl;
	file.seekg(CSA*512);
	file.read(delete_file_content, 16);
	for (int i=0; i<16; i++){
		cout<<(('!'<delete_file_content[i] && delete_file_content[i]<'~')?delete_file_content[i]:'.');
	}
	cout<<endl;

}

/**
 * @param: start - the entry of NTFS
 *
 * @brief: read the NTSF partition with the start offset, and print all of information
 *
 * @return: void
 */
void readNTFS(int start){
	char NTFS[84],MFT[1024];
	file.seekg(start);
	file.read(NTFS,84);  // Bytes 0x0B-0x53 are the BPB(25 bytes) and the extended BPB(48 bytes).
	int byt_per_sector = getOffset(NTFS, 0x0B, 2);//0x0B	word   Bytes Per Sector
	int sec_per_cluster=(int)(unsigned char)NTFS[0x0D]; //0x0D byte  Sectors Per Cluster
	//0x30	long long(16 byte) Logical Cluster Number for the file $MFT
	long long addr;
	// do the left shift operation
	for(int i=7; i>=0; i--){
		if (i==7){
			addr=(int)(unsigned char)NTFS[0x30+i];
		}else{
			addr= addr<<8 | ((int)(unsigned char)NTFS[0x30+i]  & 0x000000ff) ;

		}
	}
	long sec_address = addr * sec_per_cluster;
	file.seekg(start+(sec_address * 512));
	file.read(MFT,1024);
	int fir_attri =  getOffset(MFT, 20, 2);
	int f_length = getOffset(MFT, fir_attri+4, 4);
	int f_type = getOffset(MFT, fir_attri, 4);
	int sec_attri = f_length + fir_attri;
	int s_length = getOffset(MFT, sec_attri+4, 4);
	int s_type = getOffset(MFT, sec_attri, 4);

	cout<<"\n~~~~~~~~~~~~~~~~~~~~~NTFS information~~~~~~~~~~~~~~~~~~~~~"<<endl;
	cout<<"The bytes per sector for this NTFS volume: "<<byt_per_sector<<endl;
	cout<<"The sectors per cluster for this NTFS volume: "<<sec_per_cluster<<endl;
	cout<<"The sector address for the $MFT file record: "<<sec_address<<endl;
	cout<<"The type and length of the first attribute in the $MFT record:  "<<endl;
	cout<<"type:  "<<f_type<<"\t length:  "<<f_length<<endl;
	cout<<"The type and length of the second attribute in the $MFT record:  "<<endl;
	cout<<"type:  "<<s_type<<"\t length:  "<<s_length<<endl;
}

int main() {

	char File_path[]="";
	string FilePath;
	bool NTFScount=false;
	bool FAT16count=false;
	char esc;
	cout <<"Please input the path of Image file: "<<endl;
	cin >> File_path;
	cout<<"----------------------------------------------------------"<<endl;
	readFile(File_path);
	readMBR();
	//count the number of FAT partition
	for (int i=0; i<4; i++){
		if (parts[i].type.compare("FAT-16")==0){
			readFAT16(parts[i].start * 512);
			FAT16count=true;
		}
	}
	//count the number of NTFS partition
	for (int i=0; i<4; i++){
			if (parts[i].type.compare("NTFS")==0){
				NTFScount=true;
				readNTFS(parts[i].start * 512);
			}
		}
	if (!NTFScount){
		cout<<"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<endl;
		cout<<"There is no NTFS file system!"<<endl;
	}else if (!FAT16count){
		cout<<"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<endl;
		cout<<"There is no FAt-16 file system!"<<endl;
	}
	file.close();
	cout<<"\n~~~~~~~~~~~~~~~~~~~~~~~~~~END~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<endl;
	while(1){
		cout<<"please input q to exit this program!"<<endl;
		cin>>esc;
		if (esc == 'q'){
			break;
		}else{
			cout<<"error input!"<<endl;
		}
	}
	return 0;
}
