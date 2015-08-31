// n3mtodae.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <Share.h>
#include <math.h>
#define TILE_DOWNLOAD_URL "1.maptile.lbs.ovi.com/maptiler/v2/maptile/newest/satellite.day/"
#define URL_SUFFIX "jpg?token=fee2f2a877fd4a429f17207a57658582&app_id=nokiaMaps"
#define TEXTURE_URL "maps3d.svc.nokia.com/data4"
#define MAX_CHAR 255
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
using namespace std;

struct xyz
{
	float x, y, z;
};

struct uv
{
	float u, v;
};

struct triangle
{
	unsigned short a, b, c;
};

struct source
{
	int number_of_verts;

	struct xyz *xyz_coords;
	struct uv *uv_coords;
};

struct material
{
	int number_of_tris;
	int source_index;
	int bucket_levels ;
	int bucket_count ;
	char texture_name[300] ;
	char texture[500] ;
	bool is_sat ;

	unsigned short *bucketOffsets ;
	struct triangle *tris;
};

struct model
{
	int bucket_levels;  //32
	int number_of_sources;
	int number_of_materials;

	struct source *sources;
	struct material *materials;

	float center[3] ;
	float radius[3] ;
	float matrixFloat[16] ;
	double matrixDouble[16] ;
};

bool version2OrNewer = false ;
bool version4OrNewer = false ;

struct xyz crossproduct(struct xyz a, struct xyz b)
{
	struct xyz vector;
	vector.x = a.y*b.z - b.y*a.z ; //(Ay*Bz)-(By*Az);
	vector.y = -(a.x*b.z)+(b.x*a.z) ; //-(Ax*Bz)+(Bx*Az);
	vector.z = (a.x*b.y) - (a.y*b.x) ; //(Ax*By)-(Ay*Bx);
	return vector;
}
struct xyz normalize(struct xyz a)
{
	struct xyz out ;
	double magnitude = sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
	out.x = a.x/magnitude ;
	out.y = a.y/magnitude ;
	out.z = a.z/magnitude ;
	return out ;
}

struct xyz getNormal(struct xyz a, struct xyz b, struct xyz c)
{
	struct xyz normal, x1, x2 ;

	//AB vector
	x1.x = b.x - a.x ; x1.y = b.y - a.y ; x1.z = b.z - a.z ;
	//AC vector
	x2.x = c.x - a.x ; x2.y = c.y - a.y ; x2.z = c.z - a.z ;
	normal = crossproduct( x1, x2) ;
	return normalize( normal ) ;
}
int getNumDecimalDigits(int value)
{
	char myString[50];
	sprintf(myString,"%d",value);
	return strlen(myString);
}

char * obfuscateURL(int zoom, int tilex, int tiley)
{
	int bz = floor( float((zoom * 302 + 1000) / 1000) ) ;
	char bs[500], tempx[500], tempy[500] ;
	sprintf(bs, "%s/%d", TEXTURE_URL, zoom) ;
	//number of zeros
	int numOfZeros = bz - getNumDecimalDigits(tilex) ;
	for(int i=0; i< numOfZeros; i++)
		tempx[i] = '0' ;
	sprintf( &tempx[numOfZeros], "%d", tilex) ;

	numOfZeros = bz - getNumDecimalDigits(tiley) ;
	for(int i=0; i< numOfZeros; i++)
		tempy[i] = '0' ;
	sprintf( &tempy[numOfZeros], "%d", tiley) ;

	int stringLength = strlen( bs ) ;
	int bx ;
	for (bx = 0; bx < bz - 2; bx += 2)
	{
		bs[stringLength++ ] = '/' ;
		bs[stringLength++ ] = tempx[bx];
		bs[stringLength++ ] = tempx[bx + 1];
		bs[stringLength++ ] = tempy[bx];
		bs[stringLength++ ] = tempy[bx + 1] ;
	}
	for (; bx < bz - 1; bx++)
	{
		bs[stringLength++ ] = '/';
		bs[stringLength++ ] = tempx[bx];
		bs[stringLength++ ] = tempy[bx];
	}
	bs[stringLength++ ] = '/';
	bs[stringLength] = '\0';
	return bs ;
}

void parseTextureURLs( struct model *m )
{
	for( int i=0; i< m->number_of_materials; i++)
	{
		int strLength = strlen( m->materials[ i ].texture_name ) ;
		//ending in sat
		if( strLength>3 && m->materials[ i ].texture_name[ strLength - 3]=='s' && m->materials[ i ].texture_name[ strLength - 2]=='a' && m->materials[ i ].texture_name[ strLength - 1]=='t')
		{
			sprintf(  m->materials[ i ].texture, "%s%s%s", "../../tile/" , m->materials[ i ].texture_name , ".jpg" ) ;
			m->materials[ i ].is_sat = true ;
		}
		else
		{

			//starting with sat_
			//sat_15_20107_5242
			if( strLength>3 && m->materials[ i ].texture_name[0]=='s' && m->materials[ i ].texture_name[1]=='a' && m->materials[ i ].texture_name[ 2]=='t' && m->materials[ i ].texture_name[ 3]=='_')
			{
				char temp[50] ;
				int offset = 4 ;
				int tilex, tiley, zoom, twoPowerZoom ;
				int tokenIndex = strchr(&(m->materials[ i ].texture_name[offset]) , '_') - &(m->materials[ i ].texture_name[offset]) ;
				strncpy(temp, &(m->materials[ i ].texture_name[offset]), tokenIndex);
				temp[ tokenIndex] = '\0' ;
				zoom = atoi(temp) ;

				offset = offset + tokenIndex + 1 ;
				tokenIndex = strchr(&(m->materials[ i ].texture_name[offset]) , '_') - &(m->materials[ i ].texture_name[offset]) ;
				strncpy(temp, &(m->materials[ i ].texture_name[offset]), tokenIndex);
				temp[ tokenIndex] = '\0' ;
				tilex = atoi(temp) ;

				offset = offset + tokenIndex + 1 ;
				strncpy(temp, &(m->materials[ i ].texture_name[offset]), strLength - offset);
				temp[ strLength - offset ] = '\0' ;
				tiley = atoi(temp) ;

				twoPowerZoom = pow(2.0, zoom) ;
				sprintf(  m->materials[ i ].texture, "%s%d/%d/%d/256/%s" ,TILE_DOWNLOAD_URL, zoom, tiley, twoPowerZoom-1-tilex, URL_SUFFIX ) ;
				m->materials[ i ].is_sat = true ;
				sprintf( m->materials[ i ].texture_name, "%s.jpg", m->materials[ i ].texture_name ) ;
			}
			else
			{
				//b0.texture = bw(b2.url_dir + bW)
				//maps3d.svc.nokia.com/data4/13/4914/10/
				// for map_13_4918_1407_0.jpg
				char *urlPrefix ;
				char temp[50] ;
				int offset = 4 ;
				int tilex, tiley, zoom, twoPowerZoom ;
				int tokenIndex = strchr(&(m->materials[ i ].texture_name[offset]) , '_') - &(m->materials[ i ].texture_name[offset]) ;
				strncpy(temp, &(m->materials[ i ].texture_name[offset]), tokenIndex);
				temp[ tokenIndex] = '\0' ;
				zoom = atoi(temp) ;

				offset = offset + tokenIndex + 1 ;
				tokenIndex = strchr(&(m->materials[ i ].texture_name[offset]) , '_') - &(m->materials[ i ].texture_name[offset]) ;
				strncpy(temp, &(m->materials[ i ].texture_name[offset]), tokenIndex);
				temp[ tokenIndex] = '\0' ;
				tilex = atoi(temp) ;

				offset = offset + tokenIndex + 1 ;
				tokenIndex = strchr(&(m->materials[ i ].texture_name[offset]) , '_') - &(m->materials[ i ].texture_name[offset]) ;
				strncpy(temp, &(m->materials[ i ].texture_name[offset]), tokenIndex);
				temp[ tokenIndex] = '\0' ;
				tiley = atoi(temp) ;

				urlPrefix =  obfuscateURL(zoom, tilex, tiley) ;
				sprintf(  m->materials[ i ].texture, "%s%s", urlPrefix, m->materials[ i ].texture_name ) ;
				m->materials[ i ].is_sat = false ;            
			}
		}

	}
}

int open_n3m (struct model *m, void *data)
{
	char *magic = (char*)data;
	printf("magic: %c%c%c\n", magic[0], magic[1], magic[2]);
	if (magic[0] != 'N' || magic[1] != '3' || magic[2] != 'M') {
		return -1;
	}

	char version = magic[3];
	if( version >= 50 )
		version2OrNewer = true ;
	if( version >= 52 )
		version4OrNewer = true ;

	printf("version: %c\n", version);

	int *header = (int*)data;
	m->number_of_sources = header[1];
	m->number_of_materials = header[2];
	printf("number_of_sources: %d\n", m->number_of_sources);
	printf("number_of_materials: %d\n", m->number_of_materials);

	m->sources = (source *) calloc(m->number_of_sources, sizeof(struct source));
	m->materials = (material *) calloc(m->number_of_materials, sizeof(struct material));
	m->bucket_levels = 32 ;

	int i = 0;
	for (i = 0; i < m->number_of_sources; i++) {
		struct source *s = &m->sources[i];

		int offset = i * 2 + 3;

		int data_offset = header[offset];
		s->number_of_verts = header[offset + 1];

		printf("data_offset: %d\n", data_offset);
		printf("source[%d].number_of_verts: %d\n", i, s->number_of_verts);

		//xyz* tempxyz = (struct xyz*)((char*)data + data_offset); //number_of_verts*3
		//for (int j = 0; j < s->number_of_verts; j++) {
		//	xyz* temp1 = &tempxyz[j];
		//	temp1->x = 0;
		//	xyz* temp2 = new xyz();

		//	temp2->z = temp1->z;
		//	s->xyz_coords = temp2;
		//	//fprintf(f, "%g %g %g ", src->xyz_coords[j].z, src->xyz_coords[j].x, src->xyz_coords[j].y);
		//}

		s->xyz_coords= (struct xyz*)((char*)data + data_offset);

		s->uv_coords = (struct uv*)((char*)data + data_offset + s->number_of_verts * 12); //number_of_verts*2
	}

	for (i = 0; i < m->number_of_materials; i++) {
		struct material *mat = &m->materials[i];

		int offset = m->number_of_sources * 2 + i * 4 + 3;

		int indices_offset = header[offset];

		int textureNameOffset = header[ offset +3 ] ; //DONT KNOW YET WHAT IT IS

		mat->number_of_tris = header[offset + 1];		
		mat->source_index = header[offset + 2];
		mat->tris = (struct triangle*)((char*)data + indices_offset); //number_of_tris *3

		printf("material[%d].number_of_tris: %d\n", i, mat->number_of_tris);
		printf("material[%d].source_index: %d\n", i, mat->source_index);

		if(version4OrNewer)
		{
			int offset2 = indices_offset + mat->number_of_tris*6 ;
			unsigned short *matrix = (unsigned short*)( (char*)data + offset2); 

			mat->bucket_levels = matrix[0] ;
			if( mat->bucket_levels < m->bucket_levels)
				m->bucket_levels = mat->bucket_levels ;
			mat->bucket_count = 1 << (mat->bucket_levels << 1) ;

			mat->bucketOffsets = (unsigned short *) calloc(mat->bucket_count + 1, sizeof(unsigned short));
			for( int i=0; i< mat->bucket_count; i++ )
				mat->bucketOffsets[ i ] = matrix[ i+1 ] ;

			mat->bucketOffsets[ mat->bucket_count ] = mat->number_of_tris ;
		}
		else //older than version 4 pointclouds
		{
			m->bucket_levels = 0 ;
			mat->bucket_levels = 0 ;
		}
		unsigned char textureNameLength = magic[ textureNameOffset ] ;
		for(int i=0; i< textureNameLength; i++)
			mat->texture_name[i] = magic[ textureNameOffset +1 + i] ;
		mat->texture_name[textureNameLength] = '\0' ;
	}
	int center_radius_offset = header[ m->number_of_sources*2 + m->number_of_materials*4 +3] ;
	m->center[0] = ((float*)( (char*)data + center_radius_offset +64 ))[0] ; 
	m->center[1] = ((float*)( (char*)data + center_radius_offset +64 ))[1] ; 
	m->center[2] = ((float*)( (char*)data + center_radius_offset +64 ))[2] ; 

	m->radius[0] = ((float*)( (char*)data + center_radius_offset +76 ))[0] ; 
	m->radius[1] = ((float*)( (char*)data + center_radius_offset +76 ))[1] ; 
	m->radius[2] = ((float*)( (char*)data + center_radius_offset +76 ))[2] ; 

	if(version2OrNewer)
	{
		for(int i=0; i<16; i++)
			m->matrixDouble[ i ] =  ((double*)( (char*)data + center_radius_offset +88 ))[i] ;

		for(int i=0; i<16; i++)
			m->matrixFloat[ i ] =  ((float*)( (char*)data + center_radius_offset ))[i] ;
	}
	else
	{
		for(int i=0; i<16; i++)
			m->matrixFloat[ i ] =  ((float*)( (char*)data + center_radius_offset ))[i] ;
	}
	if (m->bucket_levels == 32)
		m->bucket_levels = 0 ;

	return 0;
}

int save_dae (struct model *m, FILE *f,float lon,float lat,float scale,float bottom,float top)
{
	struct xyz normal ;

	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n");
	fprintf(f, "<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"1.4.1\">\n");
	fprintf(f, "    <asset>\n");
	fprintf(f, "        <contributor>\n");
	fprintf(f, "            <authoring_tool>n3mtodae</authoring_tool>\n");
	fprintf(f, "        </contributor>\n");
	fprintf(f, "        <created>2012-01-21T04:10:34Z</created>\n");
	fprintf(f, "        <modified>2012-01-21T04:10:34Z</modified>\n");
	fprintf(f, "        <unit meter=\"1\" name=\"meter\" />\n");
	fprintf(f, "        <up_axis>X_UP</up_axis>\n");
	fprintf(f, "    </asset>\n");

	//IMPORT TEXTURE IMAGES	
	fprintf(f, "    <library_images>");
	int i = 0;
	for (i = 0; i < m->number_of_materials; i++)
	{
		struct material *mat = &m->materials[i];
		fprintf(f, "\n                <image id=\"material%p-image\" name=\"material%p-image\">", mat, mat);
		fprintf(f, "\n                    <init_from>%s</init_from>", mat->texture_name);
		fprintf(f, "\n                </image>");
	}
	fprintf(f, "\n    </library_images>\n");

	//IMPORT MATERIALS
	fprintf(f, "    <library_materials>");
	i = 0;
	for (i = 0; i < m->number_of_materials; i++)
	{
		struct material *mat = &m->materials[i];
		fprintf(f, "\n                <material id=\"material%pID\" name=\"material%p\">", mat, mat);
		fprintf(f, "\n                    <instance_effect url=\"#material%p-effect\" />", mat);
		fprintf(f, "\n                </material>");
	}
	fprintf(f, "\n    </library_materials>\n");

	//DECLARE EFFECTS
	fprintf(f, "    <library_effects>\n");
	i = 0;
	for (i = 0; i < m->number_of_materials; i++)
	{
		struct material *mat = &m->materials[i];
		fprintf(f, "        <effect id=\"material%p-effect\" name=\"material%p-effect\">\n", mat, mat);
		fprintf(f, "            <profile_COMMON>\n");
		fprintf(f, "                <newparam sid=\"material%p-image-surface\">\n", mat);
		fprintf(f, "                    <surface type=\"2D\">\n");
		fprintf(f, "                        <init_from>material%p-image</init_from>\n", mat);
		fprintf(f, "                    </surface>\n");
		fprintf(f, "                </newparam>\n");
		fprintf(f, "                <newparam sid=\"material%p-image-sampler\">\n", mat);
		fprintf(f, "                    <sampler2D>\n");
		fprintf(f, "                        <source>material%p-image-surface</source>\n", mat);
		fprintf(f, "                    </sampler2D>\n");
		fprintf(f, "                </newparam>\n");

		fprintf(f, "                <technique sid=\"COMMON\">\n");
		fprintf(f, "                    <lambert>\n");
		fprintf(f, "                        <emission>\n");
		fprintf(f, "                            <color>0.0000000 0.0000000 0.0000000 1.0000000</color>\n");
		fprintf(f, "                        </emission>\n");
		fprintf(f, "                        <ambient>\n");
		fprintf(f, "                            <color>0.0000000 0.0000000 0.0000000 1.0000000</color>\n");
		fprintf(f, "                        </ambient>\n");
		fprintf(f, "                        <diffuse>\n");
		fprintf(f, "                            <texture texture=\"material%p-image-sampler\" texcoord=\"UVSET0\"/>\n", mat);
		fprintf(f, "                        </diffuse>\n");
		fprintf(f, "                    </lambert>\n");
		fprintf(f, "                </technique>\n");
		fprintf(f, "            </profile_COMMON>\n");
		fprintf(f, "        </effect>\n");
	}
	fprintf(f, "    </library_effects>\n");


	fprintf(f, "    <library_visual_scenes>\n");
	fprintf(f, "        <visual_scene id=\"ID1VISUALSCENE\">\n");
	fprintf(f, "            <node name=\"n3mtodae\">\n");
	i = 0;
	for (i = 0; i < m->number_of_materials; i++) {
		struct material *mat = &m->materials[i];

		fprintf(f, "                <instance_geometry url=\"#ID%pGEOMETRY\">\n", mat);

		fprintf(f, "                    <bind_material>\n");
		fprintf(f, "                        <technique_common>\n");
		fprintf(f, "                            <instance_material symbol=\"material%p\" target=\"#material%pID\">\n", mat,  mat);
		fprintf(f, "                                <bind_vertex_input semantic=\"UVSET0\" input_semantic=\"TEXCOORD\" input_set=\"0\" />\n");
		fprintf(f, "                            </instance_material>\n");
		fprintf(f, "                        </technique_common>\n");
		fprintf(f, "                    </bind_material>\n");

		fprintf(f, "                </instance_geometry>\n");
	}
	fprintf(f, "            </node>\n");
	fprintf(f, "        </visual_scene>\n");
	fprintf(f, "    </library_visual_scenes>\n");

	fprintf(f, "    <library_geometries>\n");
	for (i = 0; i < m->number_of_materials; i++) {
		struct material *mat = &m->materials[i];
		struct source *src = &m->sources[mat->source_index];

		fprintf(f, "        <geometry id=\"ID%pGEOMETRY\">\n", mat);
		fprintf(f, "            <mesh>\n");


		//Write geometry position
		fprintf(f, "                <source id=\"ID%pPOSITION\">\n", src->xyz_coords);
		fprintf(f, "                    <float_array id=\"ID%pPOSITION-ARRAY\" count=\"%d\">", src->xyz_coords, src->number_of_verts * 3);
		int j = 0;
		for (j = 0; j < src->number_of_verts; j++) {
			double x = lon - scale*0.5 + scale * src->xyz_coords[j].y * 256/65536;
			double y = lat - scale*255.5 + scale * src->xyz_coords[j].z * 256/65536;
			double z = bottom + src->xyz_coords[j].x/65536 * (top-bottom);
			fprintf(f, "%f %f %f \n", x,y,z);
		}
		fprintf(f, "</float_array>\n");
		fprintf(f, "                    <technique_common>\n");
		fprintf(f, "                        <accessor count=\"%d\" source=\"#ID%pPOSITION-ARRAY\" stride=\"3\">\n", src->number_of_verts, src->xyz_coords);
		fprintf(f, "                            <param name=\"X\" type=\"float\" />\n");
		fprintf(f, "                            <param name=\"Y\" type=\"float\" />\n");
		fprintf(f, "                            <param name=\"Z\" type=\"float\" />\n");
		fprintf(f, "                        </accessor>\n");
		fprintf(f, "                    </technique_common>\n");
		fprintf(f, "                </source>\n");

		//Write geometry normals
		//3 normals per triangle
		fprintf(f, "                <source id=\"ID%pNORMALS\">\n", src->xyz_coords);
		fprintf(f, "                    <float_array id=\"ID%pNORMALS-ARRAY\" count=\"%d\">", src->xyz_coords, mat->number_of_tris * 3);
		j = 0;	
		for (j = 0; j < mat->number_of_tris; j++) {
			normal = getNormal( src->xyz_coords[ mat->tris[j].a ], src->xyz_coords[ mat->tris[j].b ], src->xyz_coords[ mat->tris[j].c ] ) ;
			fprintf(f, "%g %g %g ", normal.x, normal.y, normal.z);
		}
		fprintf(f, "</float_array>\n");
		fprintf(f, "                    <technique_common>\n");
		fprintf(f, "                        <accessor count=\"%d\" source=\"#ID%pNORMALS-ARRAY\" stride=\"3\">\n", mat->number_of_tris, src->xyz_coords);
		fprintf(f, "                            <param name=\"X\" type=\"float\" />\n");
		fprintf(f, "                            <param name=\"Y\" type=\"float\" />\n");
		fprintf(f, "                            <param name=\"Z\" type=\"float\" />\n");
		fprintf(f, "                        </accessor>\n");
		fprintf(f, "                    </technique_common>\n");
		fprintf(f, "                </source>\n");

		//Write geometry UV
		fprintf(f, "                <source id=\"ID%pUV\">\n", src->xyz_coords);
		fprintf(f, "                    <float_array id=\"ID%pUV-ARRAY\" count=\"%d\">", src->xyz_coords, src->number_of_verts * 2);
		j = 0;
		for (j = 0; j < src->number_of_verts; j++) {
			fprintf(f, "%g %g ", src->uv_coords[j].u, src->uv_coords[j].v);
		}
		fprintf(f, "</float_array>\n");
		fprintf(f, "                    <technique_common>\n");
		fprintf(f, "                        <accessor count=\"%d\" source=\"#ID%pUV-ARRAY\" stride=\"2\">\n", src->number_of_verts, src->xyz_coords);
		fprintf(f, "                            <param name=\"S\" type=\"float\" />\n");
		fprintf(f, "                            <param name=\"T\" type=\"float\" />\n");
		fprintf(f, "                        </accessor>\n");
		fprintf(f, "                    </technique_common>\n");
		fprintf(f, "                </source>\n");

		fprintf(f, "                <vertices id=\"ID%pVERTICES\">\n", mat);
		fprintf(f, "                    <input semantic=\"POSITION\" source=\"#ID%pPOSITION\" />\n", src->xyz_coords);
		fprintf(f, "                </vertices>\n");

		fprintf(f, "                <triangles count=\"%d\" material=\"material%p\">\n", mat->number_of_tris, mat);
		fprintf(f, "                    <input offset=\"0\" semantic=\"VERTEX\" source=\"#ID%pVERTICES\" />\n", mat);
		fprintf(f, "                    <input offset=\"1\" semantic=\"NORMAL\" source=\"#ID%pNORMALS\" />\n", mat);
		fprintf(f, "                    <input offset=\"2\" set=\"0\" semantic=\"TEXCOORD\" source=\"#ID%pUV\" />\n", src->xyz_coords);
		fprintf(f, "                    <p>");

		for (j = 0; j < mat->number_of_tris; j++) {
			fprintf(f, "%d %d %d %d %d %d %d %d %d ", mat->tris[j].a, j, mat->tris[j].a, mat->tris[j].b, j, mat->tris[j].b, mat->tris[j].c, j, mat->tris[j].c);
		}

		fprintf(f, "</p>\n");
		fprintf(f, "                </triangles>\n");

		fprintf(f, "            </mesh>\n");
		fprintf(f, "        </geometry>\n");
	}
	fprintf(f, "    </library_geometries>\n");


	fprintf(f, "    <scene>\n");
	fprintf(f, "        <instance_visual_scene url=\"#ID1VISUALSCENE\" />\n");
	fprintf(f, "    </scene>\n");

	fprintf(f, "</COLLADA>\n");

	return 0;
}

void downloadTextures( model *m ,char * folder)
{
	char cmd[600] ;
	
	for( int i=0; i< m->number_of_materials; i++)
	{
		sprintf( cmd, "D:\\我的程序\\开源项目\\Here3D\\wget.exe -O %s%s http://%s",folder, m->materials[i].texture_name, m->materials[i].texture ) ;
		system(cmd) ;
	}
}

void convert(model *m,float lon,float lat,float scale)
{
	
	for (int i = 0; i < m->number_of_materials; i++) {
		struct material *mat = &m->materials[i];
		struct source *src = &m->sources[mat->source_index];
		int j = 0;
		for (j = 0; j < src->number_of_verts; j++) {
			xyz* tempx = new xyz();
			tempx->x = (float)(lon - scale/2 + scale * src->xyz_coords[j].x * 256/65536);
			src->xyz_coords[j] = *tempx;
			src->xyz_coords[j].x = 0;//
			src->xyz_coords[j].y = lat + scale/2 - scale * src->xyz_coords[j].y * 256/65536;
			//fprintf(f, "%g %g %g ", src->xyz_coords[j].y, src->xyz_coords[j].z, src->xyz_coords[j].x);
		}
	}
}


int main(int argc,char *argv[])
{
	//int fd ; //= open("/Users/fak/Desktop/Nokia3D/b.n3m", O_RDONLY);
	//_sopen_s( &fd, "f_stat.out", O_RDONLY) ;

	float lon = atof(argv[4]);
	float lat = atof(argv[5]);
	float scale = atof(argv[6]);
	float bottom = atof(argv[7]);
	float top = atof(argv[8]);
	//float* lat = (float*)argv[5];
	//float* jiange =(float*) argv[6];

	int fd  = _sopen(argv[1], O_RDONLY, _SH_DENYNO);
	struct stat sb = {0};
	fstat(fd, &sb);

	//void *data = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

	HANDLE fm; 
	HANDLE h = (HANDLE) _get_osfhandle (fd); 
	fm = CreateFileMapping(  h,  NULL,  PAGE_READONLY,  0,0,  NULL); 
	void *data = static_cast<void*>(MapViewOfFile( fm,  FILE_MAP_READ,  0,  0, sb.st_size)); 

	struct model m;

	open_n3m (&m, data);
	parseTextureURLs( &m ) ;
	downloadTextures( &m,argv[3]) ;

	FILE *out = fopen(argv[2], "w");
	save_dae (&m, out,lon, lat, scale,bottom,top);

	UnmapViewOfFile(data); 
	CloseHandle(fm); 

	return 0;
}

