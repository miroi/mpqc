
#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "chemelem.h"

DescribedClass_REF_def(ChemicalElement);

#define CLASSNAME ChemicalElement
#define PARENTS virtual public SavableState
#define HAVE_STATEIN_CTOR
#include <util/state/statei.h>
#include <util/class/classi.h>
void *
ChemicalElement::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = SavableState::_castdown(cd);
  return do_castdowns(casts,cd);
}

ChemicalElement::ChemicalElement(int Z) :
  Z_(Z)
{
}

ChemicalElement::ChemicalElement(const ChemicalElement&ce) :
  Z_(ce.Z_)
{
}

ChemicalElement::ChemicalElement(const char* name)
{
  int i,j;

  // see if the name is a atomic number
  Z_ = atoi(name);

  // name is not an atomic number--must be a symbol or atom name
  if (!Z_) {

      // convert the name to lower case
      char* tmpname = strdup(name);
      for (j=0; j<strlen(tmpname); j++) {
	  if (isupper(tmpname[j])) tmpname[j] = tolower(tmpname[j]);
	}

      // loop thru the elements, looking for a match
      for (i=0; i<max_atomic_number; i++) {

          // see if an atom name matches
          // if we find a match we are done looping
	  if (!strcmp(atom_info[i].name,tmpname)) {
	      Z_ = i;
	      break;
	    }

          // see if an atomic symbol (converted to lower case) matches
          char* tmpsymbol = strdup(atom_info[i].symbol);
          for (j=0; j<strlen(tmpsymbol); j++) {
	      if (isupper(tmpsymbol[j])) tmpsymbol[j] = tolower(tmpsymbol[j]);
            }
          // if we find a match we are done looping
	  if (!strcmp(tmpsymbol,tmpname)) {
	      Z_ = i;
              free(tmpsymbol);
	      break;
	    }
          free(tmpsymbol);
	}

      // free the lowercase version of the name
      free(tmpname);
    }  

  // check to see if z value is OK, if not then the name must have been
  // invalid
  if (Z_ < 1 || Z_ > max_atomic_number) {
      fprintf(stderr,"ChemicalElement: invalid name: %s\n",name);
      exit(1);
    }
}

ChemicalElement::~ChemicalElement()
{
}

void ChemicalElement::save_data_state(StateOut& so)
{
  so.put(Z_);
}

ChemicalElement::ChemicalElement(StateIn& si):
  SavableState(si,ChemicalElement::class_desc_)
{
  si.get(Z_);
}

// Initialize static variables for ChemicalElement class
int ChemicalElement::max_atomic_number=107;
ChemicalElement::atom_info_type ChemicalElement::atom_info[]=
  {{"",            "",  0,  0.000000000, 0,0,0,   0,   0,  0,   0, 0.0, 0.00,0.00,0.00, 0.00},
   {"hydrogen",   "H",  1,  1.007825037, 1,1,1,-259,-252,313,0.25, 2.2, 0.57,0.07,0.30, 1.20},
   {"helium",     "He", 2,  4.002603250,18,1,2,-272,-269,563,0.20,-1.5,-1.50,0.15,0.93, 1.20},
   {"lithium",    "Li", 3,  7.016004500, 1,2,1, 181,1342,124,1.45, 1.0, 0.08,0.53,1.52, 1.37},
   {"beryllium",  "Be", 4,  9.012182500, 2,2,2, 127,2970,214,1.05, 1.5, 0.42,1.85,1.12, 1.45},
   {"boron",      "B",  5, 11.009305300,13,2,3,2079,2550,191,0.85, 2.0, 0.30,2.34,0.88, 1.45},
   {"carbon",     "C",  6, 12.000000000,14,2,4,3550,4827,259,0.70, 2.5, 0.17,2.25,0.77, 1.50},
   {"nitrogen",   "N",  7, 14.003074008,15,2,5,-210,-196,334,0.65, 3.0, 0.39,0.81,0.70, 1.50},
   {"oxygen",     "O",  8, 15.994914640,16,2,6,-218,-183,313,0.60, 3.5, 0.33,1.14,0.66, 1.40},
   {"fluorine",   "F",  9, 18.998403250,17,2,7,-220,-188,401,0.50, 4.0,-1.50,1.11,0.64, 1.35},
   {"neon",       "Ne",10, 19.992439100,18,2,8,-249,-246,496,0.45,-1.5,-1.50,1.21,1.31, 1.30},
   {"sodium",     "Na",11, 22.989769700, 1,3,1,  98, 883,118,1.80, 0.9, 0.30,0.97,1.86, 1.57},
   {"magnesium",  "Mg",12, 23.985045000, 2,3,2, 649,1090,176,1.50, 1.2, 0.25,1.74,1.60, 1.36},
   {"aluminum",   "Al",13, 26.981541300,13,3,3, 660,2467,138,1.25, 1.5, 0.21,2.70,1.43, 1.24},
   {"silicon",    "Si",14, 27.976928400,14,3,4,1410,2355,187,1.10, 1.8, 0.17,2.33,1.17, 2.10},
   {"phosphorus" ,"P", 15, 30.973763400,15,3,5,  44, 280,350,1.00, 2.1, 0.19,0.82,1.10, 1.80},
   {"sulfur",     "S", 16, 31.972071800,16,3,6, 113, 445,238,1.00, 2.5, 0.18,2.06,1.04, 1.75},
   {"chlorine",   "Cl",17, 34.968852729,17,3,7,-101, -35,298,1.00, 3.0, 0.19,1.37,0.99, 1.70},
   {"argon",      "Ar",18, 39.962383100,18,3,0,-189,-186,362,1.00,-1.5, 0.16,1.40,1.74,-1.00},
   {"potassium",  "K", 19, 38.963707900, 1,4,1,  64, 774,100,2.20, 0.8, 0.19,0.86,2.31,-1.00},
   {"calcium",    "Ca",20, 39.962590700, 2,4,2, 839,1484,141,1.80, 1.0, 0.15,1.54,1.97,-1.00},
   {"scandium",   "Sc",21, 44.955913600, 3,4,9,1539,2832,150,1.60, 1.3,-1.50,2.99,1.60,-1.00},
   {"titanium",   "Ti",22, 47.947946700, 4,4,9,1660,3287,157,1.40, 1.4, 0.11,4.54,1.46,-1.00},
   {"vanadium",   "V", 23, 50.943962500, 5,4,9,1890,3380,155,1.35, 1.6, 0.12,6.09,1.31,-1.00},
   {"chromium",   "Cr",24, 51.940509700, 6,4,9,1857,2672,156,1.40, 1.6, 0.11,7.18,1.25,-1.00},
   {"manganese",  "Mn",25, 54.938046300, 7,4,9,1244,1962,171,1.40, 1.5, 0.12,7.21,1.29,-1.00},
   {"iron",       "Fe",26, 55.934939300, 8,4,9,1535,2750,181,1.40, 1.8, 0.11,7.87,1.26,-1.00},
   {"cobalt",     "Co",27, 58.933197800, 9,4,9,1495,2870,181,1.35, 1.8, 0.10,8.90,1.25,-1.00},
   {"nickel",     "Ni",28, 57.935347100,10,4,9,1453,2732,176,1.35, 1.8, 0.11,8.90,1.24,-1.00},
   {"copper",     "Cu",29, 62.929599200,11,4,9,1083,2567,178,1.35, 1.9, 0.09,8.96,1.20,-1.00},
   {"zinc",       "Zn",30, 63.929145400,12,4,9, 420, 907,216,1.35, 1.6, 0.09,7.13,1.33,-1.00},
   {"gallium",    "Ga",31, 68.925580900,13,4,3,  30,2403,138,1.30, 1.6, 0.08,5.91,1.22,-1.00},
   {"germanium",  "Ge",32, 73.921178800,14,4,4, 937,2830,182,1.25, 1.8, 0.07,5.32,1.22,-1.00},
   {"arsenic",    "As",33, 74.921595500,15,4,5, 817, 613,226,1.15, 2.0, 0.08,5.73,1.21,-1.00},
   {"selenium",   "Se",34, 79.916520500,16,4,6, 217, 685,224,1.15, 2.4, 0.08,4.79,1.17,-1.00},
   {"bromine",    "Br",35, 78.918336100,17,4,7,  -7,  59,272,1.15, 2.8, 0.09,3.12,1.14, 2.30}, 
   {"krypton",    "Kr",36, 83.911506400,18,4,0,-157,-152,322,0.00,-1.5,-1.50,2.16,1.89,-1.00},
 //
 // the following masses are just the average atomic mass, not the mass of the
 // most common isotope
 //
   {"rubidium",     "Rb",37, 85.4678, 1,5,1,  39, 688, 96,2.35,0.8,0.08,1.53,2.44,-1.00},
   {"strontium",    "Sr",38, 87.6200, 2,5,2, 769,1384,131,2.00,1.0,0.00,2.63,2.15,-1.00},
   {"yttrium",      "Y", 39, 88.9059, 3,5,3,1523,3337,147,1.80,1.2,0.07,4.47,1.62,-1.00},
   {"zirconium",    "Zr",40, 91.2200, 4,5,4,1852,4377,157,1.55,1.4,0.07,5.80,1.48,-1.00},
   {"niobium",      "Nb",41, 92.9064, 5,5,5,2468,4742,158,1.45,1.6,0.06,7.83,1.48,-1.00},
   {"molybdenum",   "Mo",42, 95.9400, 6,5,6,2617,4612,163,1.45,1.8,0.06,9.35,1.47,-1.00},
   {"technetium",   "Tc",43, 98.0000, 7,5,7,2172,4877,167,1.35,1.9,0.06,11.5,1.47,-1.00},
   {"ruthenium",    "Ru",44,101.0700, 8,5,8,2310,3900,170,1.30,2.2,0.06,10.9,1.46,-1.00},
   {"rhodium",      "Rh",45,102.9055, 9,5,6,1966,3727,172,1.35,2.2,0.07,11.1,1.45,-1.00},
   {"palladium",    "Pd",46,106.4000,10,5,4,1554,3140,192,1.40,2.2,0.06,10.7,1.44,-1.00},
   {"silver",       "Ag",47,107.8680,11,5,2, 962,2212,174,1.60,1.9,0.06,9.32,1.53,-1.00},
   {"cadminium",    "Cd",48,112.4100,12,5,2, 321, 765,207,1.55,1.7,0.06,8.02,1.48,-1.00},
   {"indium",       "In",49,114.8200,13,5,3, 157,2080,133,1.55,1.7,0.06,7.30,1.44,-1.00},
   {"tin",          "Sn",50,118.6900,14,5,4, 232,2270,169,1.45,1.8,0.05,6.98,1.40, 2.15},
   {"antimony",     "Sb",51,121.7500,15,5,5, 631,1750,199,1.45,1.9,0.05,6.69,1.41,-1.00},
   {"tellurium",    "Te",52,127.6000,16,5,6, 450, 990,207,1.40,2.1,0.05,5.75,1.37,-1.00},
   {"iodine",       "I", 53,126.9045,17,5,7, 114, 184,240,1.40,2.5,0.01,4.94,1.33, 2.15},
   {"xenon",        "Xe",54,131.3000,18,5,0,-112,-107,279,0.00,-1.5,0.04,0.00,2.09,-1.00},
   {"cesium",       "Cs",55,132.9054, 1,6,1,  28, 669, 90,2.60,0.7,0.06,1.84,2.62,-1.00},
   {"barium",       "Ba",56,137.3300, 2,6,2, 725,1640,120,2.15,0.9,0.05,3.50,2.17,-1.00},
   {"lanthanium",   "La",57,138.9055, 1,6,9, 920,3454,128,1.95,1.1,0.05,6.20,1.69,-1.00},
   {"cerium",       "Ce",58,140.1200, 2,8,9, 798,3257,126,1.85,1.1,0.05,6.70,-1.5,-1.00},
   {"praseodymium", "Pr",59,140.9077, 3,8,3, 931,3212,125,1.85,1.2,0.05,6.80,-1.5,-1.00},
   {"neodymium",    "Nd",60,144.2400, 4,8,3,1010,3127,126,1.85,1.1,0.05,7.00,-1.5,-1.00},
   {"promethium",   "Pm",61,145.0000, 5,8,3,1080,2460,128,1.85,1.1,0.04,0.00,-1.5,-1.00},
   {"samarium",     "Sm",62,150.4000, 6,8,2,1072,1778,129,1.85,1.2,0.04,7.50,-1.5,-1.00},
   {"europium",     "Eu",63,151.9600, 7,8,2, 822,1597,130,1.85,1.2,0.04,5.30,-1.5,-1.00},
   {"gadolinium",   "Gd",64,157.2500, 8,8,3,1311,3233,141,1.80,1.1,0.06,7.90,-1.5,-1.00},
   {"terbium",      "Tb",65,158.9254, 9,8,3,1360,3041,135,1.75,1.2,0.04,8.30,-1.5,-1.00},
   {"dysprosium",   "Dy",66,162.5000,10,8,3,1409,2335,136,1.75,1.2,0.04,8.50,-1.5,-1.00},
   {"holmium",      "Ho",67,164.9304,11,8,3,1470,2720,138,1.75,1.1,0.04,8.80,-1.5,-1.00},
   {"erbium",       "Er",68,167.2600,12,8,3,1522,2510,140,1.75,1.2,0.04,9.10,-1.5,-1.00},
   {"thulium",      "Tm",69,168.9342,13,8,2,1545,1727,142,1.75,1.2,0.04,9.10,-1.5,-1.00},
   {"ytterbium",    "Yb",70,173.0400,14,8,2, 824,1193,144,1.75,1.2,0.03,7.00,-1.5,-1.00},
   {"lutetium",     "Lu",71,174.9670,15,8,3,1656,3315,125,1.75,1.2,0.04,9.80,-1.5,-1.00},
   {"hafnium",      "Hf",72,178.4900, 4,6,4,2227,4602,161,1.55,1.3,0.04,13.1,1.65,-1.00},
   {"tantalum",     "Ta",73,180.9479, 5,6,5,2996,5425,181,1.45,1.5,0.03,16.6,1.62,-1.00},
   {"tungsten",     "W", 74,183.8500, 6,6,6,3410,5660,184,1.35,1.7,0.03,19.3,1.60,-1.00},
   {"rhenium",      "Re",75,186.2070, 7,6,7,3180,5627,181,1.35,1.9,0.03,21.0,1.56,-1.00},
   {"osmium",       "Os",76,190.2000, 8,6,8,3045,5027,200,1.30,2.2,0.03,22.6,1.52,-1.00},
   {"iridium",      "Ir",77,192.2200, 9,6,3,2410,4130,209,1.35,2.2,0.03,22.5,1.48,-1.00},
   {"platinum",     "Pt",78,195.0900,10,6,4,1772,3827,207,1.35,2.2,0.03,21.4,1.45,-1.00},
   {"gold",         "Au",79,196.9665,11,6,1,1064,3080,212,1.35,2.4,0.03,19.3,1.50,-1.00},
   {"mercury",      "Hg",80,200.5900,12,6,1, -39, 357,240,1.50,1.9,0.03,13.6,1.49,-1.00},
   {"thallium",     "Tl",81,204.3700,13,6,3, 304,1457,140,1.90,1.8,0.03,11.9,1.48,-1.00},
   {"lead",         "Pb",82,207.2000,14,6,4, 328,1720,171,1.80,1.8,0.04,11.4,1.75,-1.00},
   {"bismuth",      "Bi",83,208.9804,15,6,5, 271,1560,168,1.60,1.9,0.03,9.80,1.46,-1.00},
   {"polonium",     "Po",84,209.0000,16,6,6, 254, 962,194,1.90,2.0,0.03,9.20,1.40,-1.00},
   {"astatine",     "At",85,210.0000,17,6,7, 302, 337, -1,0.00,2.2,-1.5,-1.5,1.40,-1.00},
   {"radon",        "Rn",86,222.0000,18,6,0, -71, -62,247,0.00,-1.5,0.02,-1.5,2.14,-1.00},
   {"francium",     "Fr",87,223.0000, 1,7,1,  27, 677, -1,0.00,0.7,-1.5,-1.5,2.70,-1.00},
   {"radium",       "Ra",88,226.0254, 2,7,2, 700,1140,121,2.15,0.9,0.03,-1.5,2.20,-1.00},
   {"actinium",     "Ac",89,227.0278, 1,7,9,1050,3200,159,1.95,-1.5,-1.5,10.1,-1.5,-1.00},
   {"thorium",      "Th",90,232.0381, 2,9,4,1750,4790, -1,1.80,-1.5,0.03,11.7,-1.5,-1.00},
   {"protactinium", "Pa",91,231.0359, 3,9,4,1600,-273, -1,1.80,-1.5,0.03,15.4,-1.5,-1.00},
   {"uranium",      "U", 92,238.0290, 4,9,6,1132,3818, -1,1.75,-1.5,0.03,19.1,-1.5,-1.00},
   {"neptunium",    "Np",93,237.0482, 5,9,4, 640,3902, -1,1.75,-1.5,-1.5,19.5,-1.5,-1.00},
   {"plutonium",    "Pu",94,244.0000, 6,9,5, 641,3232,133,1.75,-1.5,-1.5,19.5,-1.5,-1.00},
   {"americium",    "Am",95,243.0000, 7,9,9, 994,2607,138,1.75,-1.5,-1.5,11.7,-1.5,-1.00},
   {"curium",       "Cm",96,247.0000, 8,9,3,1340,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"berkelium",    "Bk",97,247.0000, 9,9,3,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"californium",  "Cf",98,251.0000,10,9,9,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"einsteinum",   "Es",99,254.0000,11,9,9,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"fermium",      "Fm",100,257.0000,12,9,9,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"mendelevium",  "Md",101,258.0000,13,9,3,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"nobelium",     "No",102,259.0000,14,9,4,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"lawrencium",   "Lr",103,260.0000,15,9,3,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"rutherfordium","Rf",104,260.0000, 4,7,9,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"hahnium",      "Ha",105,260.0000, 5,7,9,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"Unnamed",      "Un",106,263.0000, 6,7,9,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00},
   {"Unnamed",      "Un",107,266.0000, 7,7,9,-273,-273, -1,0.00,-1.5,-1.5,-1.5,-1.5,-1.00}};
