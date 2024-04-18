#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <conio.h>
/*
DRAW. "Dessine". Prend une image BMP en entré, et la redessine comme si elle était sortie d'une bande dessinée.
Syntaxe :
	draw <input.bpm> <output.bmp>
Détails :
	Ne supporte que les fichiers BMP non compressé.
	D'une taille pouvant aller de 1 par 1 pixel à 4294967295 par 4294967295 pixels (exactement).
	Je ne m'embete pas à faire de Makefile car la commande de compilation est simple et marche normalement
	sur n'importe quel OS : gcc main.c -o draw
	Il faut bien sur avoir installé GCC.

	Si les résultats ne vous conviennent pas, essayez de jouer avec la valeur de SEUIL si dessous.
	Je l'ai mis par défaut à 30, mais elle peut aller de 0 à 255.
*/

#define SEUIL 30

unsigned char *data; // Pointeur vers les données brutes du fichier .bmp, ne pas confondre avec le tableau de pixels.
unsigned char *brightArray; // Données des moyennes des composantes pour la détection de contour. (8 bits par pixels)
long len; // Longueur en octet du fichier .bmp.

unsigned int width; // Largeur de l'image
unsigned int height; // Longueur de l'image
unsigned int imageDataOffset; // Décalage des données de l'image par apport au pointeur du fichier. (24 bits par pixel. Padding de 1 octets à chaques lignes)

#define IMAGEDATAOFFSET 0xA
#define IMAGEWIDTHOFFSET 0x12
#define IMAGEHEIGHTOFFSET 0x16
#define BITSPERPIXELSOFFSET 0x1C
#define COMPRESSMETHODOFFSET 0x1E

#define BRIGHTDATA(x,y) brightArray[(x)+((y)*width)] // Par mesure de praticité, je préfère vraiment garder celle ci dans un define

unsigned int getPixelOffset(unsigned int x, unsigned int y){
	unsigned int realy = (height-y-1); // Je convertis les coordonnées pour que y=0 corresponde au haut de l'image.
	return (imageDataOffset+ // Décalage pour se trouver au niveau des données de l'image.
		(x*3)+				 // 3 fois x, car chaque pixels possède 3 octets.
		realy+    			 // Valeur pour le padding de 1 octet à chaque ligne. Donc le total d'octet de padding vaut exactement y
		(realy*width*3)); 	 // Valeur pour le décalage en fonction de l'ordonnée. Une fois y = Width de décalage * 3
}


int main(int argc, char const *argv[])
{

	if(argc!=3){
		printf("Nombre d'argument incorrect. (%d au lieu de 2)\n", argc-1);
		return -1;
	}

	FILE *inputFile = fopen(argv[1], "rb");
	if(inputFile==NULL){
		printf("Erreur lors de l'ouverture du fichier d'entree.\n");
		return -1;
	}

	//Récupération de la longueur du fichier
	fseek(inputFile, 0, SEEK_END);
	len = ftell(inputFile);

	data = malloc(len); // Alloue de la RAM.
	fseek(inputFile, 0, SEEK_SET);
	if(fread(data, 1, len, inputFile) < len){
		printf("Erreur lors de la lecture du fichier d'entree.\n");
		fclose(inputFile);
		return -1;
	}
	fclose(inputFile);


	// Les quatres lignes qui suivent récupèrent des informations en allant lire l'header du fichier
	// Comme ceci, je ne passe par aucune librairie de traitement d'image.
	// Toutes les infos sur le format BMP sont sur wikipedia
	imageDataOffset = *(unsigned int *)(data+IMAGEDATAOFFSET);
	width = *(unsigned int *)(data+IMAGEWIDTHOFFSET);
	height = *(unsigned int *)(data+IMAGEHEIGHTOFFSET);
	unsigned short bpp = *(unsigned short *)(data+BITSPERPIXELSOFFSET); // Nombre de bits par pixels. 
	unsigned int compressMethod = *(unsigned int *)(data+COMPRESSMETHODOFFSET); // Methode de compression utilisée. (0 si compression absente.)

	brightArray = malloc(width*height); // J'ai besoin d'allouer encore de la RAM pour stocker le tableau de nuances de gris. Bon on est pas à un 1Mo près
	
	if(compressMethod!=0){ // J'aimerais bien comprendre comment marche la compression de Huffman, mais j'ai toujours pas compris.
		printf("Erreur : Le programme ne supporte pas les algorithmes de compression.\nEssayez avec une autre image, ou defenestrez votre ordinateur.\n");
		return -1;
	}
	// Si l'image de ne fait pas 24 bits par pixel, c'est même pas la peine, 
	// flemme de tout recoder pour s'adapter aux rares formats 256 couleurs ou 32 bits par pixel.
	if(bpp!=24){ 
		printf("Qui utilise encore des images avec %d bits par pixel, de nos jours ? Pff.\n", bpp);
		return -1;
	}


	printf("Width : %d Height : %d\n", width, height);

	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			unsigned int off = getPixelOffset(x,y);
			unsigned int moy = (data[off]+data[off+1]+data[off+2])/3;
			BRIGHTDATA(x,y)=moy;
			// Couleur de l'arrière plan. R:255 G:255 B:255, soit du blanc.
			data[off]=255;
			data[off+1]=255;
			data[off+2]=255;
		}
	}

	for(int x = 1; x < width-1; x++){
		for(int y = 1; y < height-1; y++){
			// Ces lignes utilisent une methode dont j'ai oublié le nom pour determiner si le pixel fait partie des contours
			// en regardant la valeur des pixels l'entourant.
			double value =  sqrt(	pow(BRIGHTDATA(x+1, y)-BRIGHTDATA(x-1, y), 2)+
									pow(BRIGHTDATA(x, y+1)-BRIGHTDATA(x, y-1), 2)+
									pow(BRIGHTDATA(x+1, y+1)-BRIGHTDATA(x-1, y-1), 2)+
									pow(BRIGHTDATA(x+1, y-1)-BRIGHTDATA(x-1, y+1), 2));
			if(value>SEUIL){ // Si la valeur obtenue dépasse un seuil, alors il fais partie des contours.
				unsigned int off = getPixelOffset(x,y);
				// Couleur des contours, R:0 G:0 B:0, soit du noir.
				data[off]=0;
				data[off+1]=0;
				data[off+2]=0;
			}

		}
	}
	
	if(access(argv[2], F_OK)==0){
		printf("ATTENTION Le fichier existe deja, si vous continuez, il sera detruit.\nEtes vous sur ? (Y/N)");
		char c=getch();
		printf("\n");
		if(!(c=='Y' || c=='y'))
			return 0;
	}

	FILE *outputFile = fopen(argv[2], "wb");
	if(outputFile==NULL){
		printf("Erreur lors de l'écriture du fichier. Etes vous sur d'avoir les permissions ?\n");
		return -1;
	}
	fseek(outputFile, 0, SEEK_SET);
	if(fwrite(data, 1, len, outputFile)<len){
		printf("Erreur lors de l'écriture du fichier. Il est maintenant potentiellement corrompu.\n");
		fclose(outputFile);
		return -1;
	}
	fclose(outputFile);

	free(brightArray);
	free(data);
	printf("OK.\n");

	return 0;
}