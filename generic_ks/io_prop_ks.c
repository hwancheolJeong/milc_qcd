/* io_prop_ks.c -- reads and writes KS quark propagators
   MIMD version 6
   MBW: 21 Feb 2001 -- just ASCII format for now, modified from
                      routines in ../generic/io_lat4.c
   MBW: Apr 2002 -- adding binary I/O
*/
/* This version assumes internal storage is at the prevailing
   precision, but the files are always 32 bit.  This code
   converts to and from the prevailing precision.  CD 11/29/04 */

#include "generic_ks_includes.h"
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "../include/io_lat.h" /* for utilities like get_f ,etc */
#include "../include/io_prop_ks.h"

#ifndef HAVE_FSEEKO
#define fseeko fseek
#endif

#define PARALLEL 1
#define SERIAL 0

#define NATURAL_ORDER 0

#undef MAX_BUF_LENGTH
#define MAX_BUF_LENGTH 4096

/* The send buffer would normally be the size of a single SU(3) vector
   but we may need to pad for short messages generated in serial reads
   and writes to avoid switch inefficiencies.  In that case we define
   PAD_SEND_BUF on the compilation line to increase this.  */
   
#ifndef PAD_SEND_BUF
#define PAD_SEND_BUF 8
#endif

/*---------------------------------------------------------------------------*/
/* Convert (or copy) one single precision su3_vector to generic precision */

void f2d_vec(fsu3_vector *a, su3_vector *b){
  int i;
  
  for(i = 0; i < 3; i++){
      b->c[i].real = a->c[i].real;
      b->c[i].imag = a->c[i].imag;
  }
}

/* Convert (or copy) one generic precision su3_vector to single precision */
void d2f_vec(su3_vector *a, fsu3_vector *b){
  int i;
  
  for(i = 0; i < 3; i++){
    b->c[i].real = a->c[i].real;
    b->c[i].imag = a->c[i].imag;
  }
}

/*------------------------------------------------------------------------*/
/* This stuff is pretty identical to both gauge and wilson prop routines.
   Oh well, don't want to mess with them, so here are the new ones for me.
*/

/*----------------------------------------------------------------------*/
/* This subroutine writes the propagator header structure */
/* Serial access version */

void swrite_ks_prop_hdr(FILE *fp, ks_prop_header *ksph)
{
  char myname[] = "swrite_ks_prop_hdr";

  swrite_data(fp,(void *)&ksph->magic_number,sizeof(ksph->magic_number),
	      myname,"magic_number");
  swrite_data(fp,(void *)ksph->dims,sizeof(ksph->dims),
	      myname,"dimensions");
  swrite_data(fp,(void *)ksph->time_stamp,sizeof(ksph->time_stamp),
	      myname,"time_stamp");
  swrite_data(fp,&ksph->order,sizeof(ksph->order),
	      myname,"order");    

  /* Header byte length */

  ksph->header_bytes = sizeof(ksph->magic_number) + sizeof(ksph->dims) + 
    sizeof(ksph->time_stamp) + sizeof(ksph->order);

} /* swrite_ks_prop_hdr */

/*------------------------------------------------------------------------*/
/* Write a data item to the prop info file */
int write_ksprop_info_item( FILE *fpout,    /* ascii file pointer */
			    char *keyword,   /* keyword */
			    char *fmt,       /* output format -
						must use s, d, e, f, or g */
			    char *src,       /* address of starting data
						floating point data must be
						of type (Real) */
			    int count,       /* number of data items if > 1 */
			    int stride)      /* byte stride of data if
						count > 1 */
{

  int i,k,n;
  char *data;

  /* Check for valid keyword */

  for(i=0;strlen(ks_prop_info_keyword[i])>0 &&
      strcmp(ks_prop_info_keyword[i],keyword) != 0; i++);
  if(strlen(ks_prop_info_keyword[i])==0)
    printf("write_ksprop_info_item: WARNING: keyword %s not in table\n",
	    keyword);

  /* Write keyword */

  fprintf(fpout,"%s",keyword);

  /* Write count if more than one item */
  if(count > 1)
    fprintf(fpout,"[%d]",count);

  n = count; if(n==0)n = 1;
  
  /* Write data */
  for(k = 0, data = (char *)src; k < n; k++, data += stride)
    {
      fprintf(fpout," ");
      if(strstr(fmt,"s") != NULL)
	fprintf(fpout,fmt,data);
      else if(strstr(fmt,"d") != NULL)
	fprintf(fpout,fmt,*(int *)data);
      else if(strstr(fmt,"e") != NULL)
	fprintf(fpout,fmt,(double)(*(Real *)data));
      else if(strstr(fmt,"f") != NULL)
	fprintf(fpout,fmt,(double)(*(Real *)data));
      else if(strstr(fmt,"g") != NULL)
	fprintf(fpout,fmt,(double)(*(Real *)data));
      else
	{
	  printf("write_ksprop_info_item: Unrecognized data type %s\n",fmt);
	  return 1;
	}
    }
  fprintf(fpout,"\n");
  return 0;

} /* end write_ksprop_info_item() */

/*----------------------------------------------------------------------*/
/* Open, write, and close the ASCII info file */

void write_ksprop_info_file(ks_prop_file *pf)
{
  FILE *info_fp;
  ks_prop_header *ph;
  char info_filename[256];
  char sums[20];

  ph = pf->header;

  /* Construct header file name from propagator file name 
   by adding filename extension to propagator file name */

  strcpy(info_filename,pf->filename);
  strcat(info_filename,ASCII_PROP_INFO_EXT);

  /* Open header file */
  
  if((info_fp = fopen(info_filename,"w")) == NULL)
    {
      printf("write_ksprop_info_file: Can't open ascii info file %s\n",
	     info_filename);
      return;
    }
  
  /* Write required information */

  write_ksprop_info_item(info_fp,"magic_number","%d",(char *)&ph->magic_number,0,0);
  write_ksprop_info_item(info_fp,"time_stamp","\"%s\"",ph->time_stamp,0,0);
  sprintf(sums,"%x %x",pf->check.sum29,pf->check.sum31);
  write_ksprop_info_item(info_fp,"checksums","\"%s\"",sums,0,0);
  write_ksprop_info_item(info_fp,"nx","%d",(char *)&nx,0,0);
  write_ksprop_info_item(info_fp,"ny","%d",(char *)&ny,0,0);
  write_ksprop_info_item(info_fp,"nz","%d",(char *)&nz,0,0);
  write_ksprop_info_item(info_fp,"nt","%d",(char *)&nt,0,0);

  write_appl_ksprop_info(info_fp);

  fclose(info_fp);

  printf("Wrote info file %s\n",info_filename); 
  fflush(stdout);

} /*write_ksprop_info_file */

/*----------------------------------------------------------------------*/

/* Set up the input prop file and prop header structures */

ks_prop_file *setup_input_ksprop_file(char *filename)
{
  ks_prop_file *pf;
  ks_prop_header *ph;

  /* Allocate space for the file structure */

  pf = (ks_prop_file *)malloc(sizeof(ks_prop_file));
  if(pf == NULL)
    {
      printf("setup_input_ksprop_file: Can't malloc pf\n");
      terminate(1);
    }

  pf->filename = filename;

  /* Allocate space for the header */

  /* Make sure compilation gave us a 32 bit integer type */
  assert(sizeof(int32type) == 4);

  ph = (ks_prop_header *)malloc(sizeof(ks_prop_header));
  if(ph == NULL)
    {
      printf("setup_input_ksprop_file: Can't malloc ph\n");
      terminate(1);
    }

  pf->header = ph;
  pf->check.sum29 = 0;
  pf->check.sum31 = 0;

  return pf;
}

/*----------------------------------------------------------------------*/

/* Set up the output prop file an prop header structure */

ks_prop_file *setup_output_ksprop_file()
{
  ks_prop_file *pf;
  ks_prop_header *ph;
  time_t time_stamp;
  int i;

  /* Allocate space for a new file structure */

  pf = (ks_prop_file *)malloc(sizeof(ks_prop_file));
  if(pf == NULL)
    {
      printf("setup_ksprop_header: Can't malloc pf\n");
      terminate(1);
    }

  /* Allocate space for a new header structure */

  ph = (ks_prop_header *)malloc(sizeof(ks_prop_header));
  if(ph == NULL)
    {
      printf("setup_ksprop_header: Can't malloc ph\n");
      terminate(1);
    }

  /* Load header pointer and file name */
  pf->header = ph;

  /* Initialize */
  pf->check.sum29 = 0;
  pf->check.sum31 = 0;

  /* Load header values */

  ph->magic_number = KSPROP_VERSION_NUMBER;

  ph->dims[0] = nx;
  ph->dims[1] = ny;
  ph->dims[2] = nz;
  ph->dims[3] = nt;

  /* Get date and time stamp. (We use local time on node 0) */

  if(this_node==0)
    {
      time(&time_stamp);
      strcpy(ph->time_stamp,ctime(&time_stamp));
      /* For aesthetic reasons, don't leave trailing junk bytes here to be
	 written to the file */
      for(i = strlen(ph->time_stamp) + 1; i < (int)sizeof(ph->time_stamp); i++)
	ph->time_stamp[i] = '\0';
      
      /* Remove trailing end-of-line character */
      if(ph->time_stamp[strlen(ph->time_stamp) - 1] == '\n')
	ph->time_stamp[strlen(ph->time_stamp) - 1] = '\0';
    }
  
  /* Broadcast to all nodes */
  broadcast_bytes(ph->time_stamp,sizeof(ph->time_stamp));

  return pf;

} /* setup_output_prop_file */

/*----------------------------------------------------------------------*/

/* Open a binary file for serial writing by node 0 */

ks_prop_file *w_serial_ks_i(char *filename)
{
  /* Only node 0 opens the file filename */
  /* Returns a file structure describing the opened file */

  FILE *fp;
  ks_prop_file *kspf;
  ks_prop_header *ksph;

  /* Set up ks_prop file and ks_prop header structs and load header values */
  kspf = setup_output_ksprop_file();
  ksph = kspf->header;

  /* Indicate coordinate natural ordering */

  ksph->order = NATURAL_ORDER;

  /* Only node 0 opens the requested file */

  if(this_node == 0)
    {
      fp = fopen(filename, "wb");
      if(fp == NULL)
	{
	  printf("w_serial_ks_i: Node %d can't open file %s, error %d\n",
		 this_node,filename,errno);fflush(stdout);
	  terminate(1);
	}

/*      printf("Opened prop file %s for serial writing\n",filename); */
      
      /* Node 0 writes the header */
      
      swrite_ks_prop_hdr(fp,ksph);

    }
  
  /* Assign values to file structure */

  if(this_node==0) kspf->fp = fp; 
  else kspf->fp = NULL;                /* Only node 0 knows about this file */

  kspf->filename       = filename;
  kspf->byterevflag    = 0;            /* Not used for writing */
  kspf->rank2rcv       = NULL;         /* Not used for writing */
  kspf->parallel       = SERIAL;

  /* Node 0 writes ascii info file */

  if(this_node == 0) write_ksprop_info_file(kspf);

  return kspf;

} /* w_serial_ks_i */

/*---------------------------------------------------------------------------*/
/* Write checksum to lattice file.  It is assumed that the file
   is already correctly positioned.

   Should be called only by one node */

void write_checksum_ks(int parallel, ks_prop_file *kspf)
{

  char myname[] = "write_checksum";

  pswrite_data(parallel,kspf->fp,
	       &kspf->check.color,sizeof(kspf->check.color),myname,"checksum");
  pswrite_data(parallel,kspf->fp,
	       &kspf->check.sum29,sizeof(kspf->check.sum29),myname,"checksum");
  pswrite_data(parallel,kspf->fp,
	       &kspf->check.sum31,sizeof(kspf->check.sum31),myname,"checksum");
  /* printf("Prop checksums %x %x for color %d\n", 
     kspf->check.sum29,kspf->check.sum31,kspf->check.color);*/

} /* write_checksum_ks */

/*---------------------------------------------------------------------------*/
/* Here only node 0 writes propagator to a serial file 
   The propagator is passed as 3 su3_vectors, one for each source color 
*/

void w_serial_ks(ks_prop_file *kspf, int color, field_offset src)
{
  /* kspf  = file descriptor as opened by w_serial_w_i 
     src   = field offset for propagator su3 vector (type su3_vector)  */

  FILE *fp;
  ks_prop_header *ksph;
  u_int32type *val;
  int rank29,rank31;
  fsu3_vector *pbuf;
  int fseek_return;  /* added by S.G. for large file debugging */
  struct {
    fsu3_vector ksv;
    char pad[PAD_SEND_BUF]; /* Introduced because some switches
			       perform better if message lengths are longer */
  } msg;
  int buf_length;

  register int i,j,k;
  off_t offset;             /* File stream pointer */
  off_t ks_prop_size;        /* Size of propagator blocks for all nodes */
  off_t ks_prop_check_size;  /* Size of propagator checksum record */
  off_t coord_list_size;    /* Size of coordinate list in bytes */
  off_t head_size;          /* Size of header plus coordinate list */
  off_t body_size ;         /* Size of propagator blocks for all nodes 
			      plus checksum record */
  int currentnode,newnode;
  int x,y,z,t;

  if(this_node==0)
    {
      if(kspf->parallel == PARALLEL)
	printf("w_serial_ks: Attempting serial write to file opened in parallel \n");

      pbuf = (fsu3_vector *)malloc(MAX_BUF_LENGTH*sizeof(fsu3_vector));
      if(pbuf == NULL)
	{
	  printf("w_serial_ks: Node 0 can't malloc pbuf\n"); 
	  fflush(stdout); terminate(1);
        }

      fp = kspf->fp;
      ksph = kspf->header;

      ks_prop_size = volume*sizeof(fsu3_vector);
      ks_prop_check_size = sizeof(kspf->check.color) +
	sizeof(kspf->check.sum29) + sizeof(kspf->check.sum31);
      body_size = ks_prop_size + ks_prop_check_size;

      /* No coordinate list was written because fields are to be written
	 in standard coordinate list order */
      
      coord_list_size = 0;
      head_size = ksph->header_bytes + coord_list_size;
      
      offset = head_size + body_size*color + ks_prop_check_size;

      fseek_return=fseeko(fp,offset,SEEK_SET);
      /* printf("w_serial_ks: Node %d fseek_return = %d\n",this_node,fseek_return); */
      if( fseek_return < 0 ) 
	{
	  printf("w_serial_ks: Node %d fseeko %lld failed error %d file %s\n",
		 this_node, (long long)offset, errno, kspf->filename);
	  fflush(stdout); terminate(1);
	}

    } /* end if(this_node==0) */

  /* Buffered algorithm for writing fields in serial order */
  
  /* initialize checksums */
  kspf->check.sum31 = 0;
  kspf->check.sum29 = 0;
  /* counts 32-bit words mod 29 and mod 31 in order of appearance on file */
  /* Here only node 0 uses these values */
  rank29 = sizeof(fsu3_vector)/sizeof(int32type)*sites_on_node*this_node % 29;
  rank31 = sizeof(fsu3_vector)/sizeof(int32type)*sites_on_node*this_node % 31;

  g_sync();
  currentnode=0;

  buf_length = 0;

  for(j=0,t=0;t<nt;t++)for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++,j++)
    {
      newnode=node_number(x,y,z,t);
      if(newnode != currentnode){	/* switch to another node */
	/* Send a few bytes of garbage to tell newnode it's OK to send */
	if( this_node==0 && newnode!=0 )
	  send_field((char *)&msg,sizeof(msg),newnode);
	if( this_node==newnode && newnode!=0 )
	  get_field((char *)&msg,sizeof(msg),0);
	currentnode=newnode;
      }
      
      if(this_node==0)
	{
	  if(currentnode==0)
	    {
	      i=node_index(x,y,z,t);
	      /* Copy or convert from structure to msg */
	      d2f_vec((su3_vector *)F_PT(&lattice[i],src), &msg.ksv);
	    }
	  else
	    {
	      get_field((char *)&msg, sizeof(msg),currentnode);
	    }

	  pbuf[buf_length] = msg.ksv;

	  /* Accumulate checksums - contribution from next site */
	  for(k = 0, val = (u_int32type *)&pbuf[buf_length]; 
	      k < (int)sizeof(fsu3_vector)/(int)sizeof(int32type); k++, val++)
	    {
	      kspf->check.sum29 ^= (*val)<<rank29 | (*val)>>(32-rank29);
	      kspf->check.sum31 ^= (*val)<<rank31 | (*val)>>(32-rank31);
	      rank29++; if(rank29 >= 29)rank29 = 0;
	      rank31++; if(rank31 >= 31)rank31 = 0;
	    }

	  buf_length++;
	  
	  if( (buf_length == MAX_BUF_LENGTH) || (j == volume-1))
	    {
	      /* write out buffer */
	      
	      if( (int)fwrite(pbuf,sizeof(fsu3_vector),buf_length,fp) 
		  != buf_length)
		{
		  printf("w_serial_ks: Node %d prop write error %d file %s\n",
			 this_node,errno,kspf->filename); 
		  fflush(stdout);
		  terminate(1);   
		}
	      buf_length = 0;		/* start again after write */
	    }
	}
      else  /* for nodes other than 0 */
	{	
	  if(this_node==currentnode){
	    i=node_index(x,y,z,t);
	    /* Copy data into send buffer and send to node 0 with padding */
	    d2f_vec((su3_vector *)F_PT(&lattice[i],src), &msg.ksv);
	    send_field((char *)&msg, sizeof(msg),0);
	  }
	}
      
    } /*close x,y,z,t loops */
  
  g_sync();

  if(this_node==0)
    {
/*      printf("Wrote prop serially to file %s\n", kspf->filename); 
	fflush(stdout); */
      free(pbuf);

      /* Construct check record */

      /* Convert to 32 bit integer */
      kspf->check.color = color;

      /* Position file pointer for writing check record */

      offset = head_size + body_size*color;
      if( fseeko(fp,offset,SEEK_SET) < 0 ) 
	{
	  printf("w_serial_ks: Node %d fseeko %lld failed error %d file %s\n",
		 this_node,(long long)offset,errno,kspf->filename);
	  fflush(stdout); terminate(1);   
	}
      
      write_checksum_ks(SERIAL,kspf);
    }

} /* w_serial_ks */

/*---------------------------------------------------------------------------*/

void w_serial_ks_f(ks_prop_file *kspf)

/* this subroutine closes the file and frees associated structures */
{
  g_sync();
  if(this_node==0)
    {
      if(kspf->parallel == PARALLEL)
	printf("w_serial_ks_f: Attempting serial close on file opened in parallel \n");

      printf("Wrote prop file %s time stamp %s\n", kspf->filename,
	     (kspf->header)->time_stamp);

      if(kspf->fp != NULL) fclose(kspf->fp);
    }

  /* Free header and file structures */
  free(kspf->header);
  free(kspf);

} /* w_serial_ks_f */

/*----------------------------------------------------------------------*/

int read_ks_prop_hdr(ks_prop_file *kspf, int parallel)
{
  /* parallel = 1 (TRUE) if all nodes are accessing the file */
  /*            0        for access from node 0 only */

  /* Returns byterevflag  = 0 or 1 */

  FILE *fp;
  ks_prop_header *ksph;
  int32type tmp;
  int j;
  int byterevflag;
  char myname[] = "read_ks_prop_hdr";

  fp = kspf->fp;
  ksph = kspf->header;

  /* Read and verify magic number */

  if(psread_data(parallel, fp,&ksph->magic_number,sizeof(ksph->magic_number),
	      myname,"magic number")!=0)terminate(1);

  tmp = ksph->magic_number;
  
  if(ksph->magic_number == KSPROP_VERSION_NUMBER) 
    byterevflag=0;
  else 
    {
      byterevn((int32type *)&ksph->magic_number,1);
      if(ksph->magic_number == KSPROP_VERSION_NUMBER) 
	{
	  byterevflag=1;
	  printf("Reading with byte reversal\n");
	  if( sizeof(Real) != sizeof(int32type)) {
	    printf("%s: Can't byte reverse\n",myname);
	    printf("requires size of int32type(%d) = size of Real(%d)\n",
		   (int)sizeof(int32type),(int)sizeof(Real));
	    terminate(1);
	  }
	}
      else
	{
	  /* Restore magic number as originally read */
	  ksph->magic_number = tmp;
	  
	  /* End of the road. */
	  printf("%s: Unrecognized magic number in prop file header.\n",
		 myname);
	  printf("Expected %x but read %x\n",
		 KSPROP_VERSION_NUMBER,tmp);
	  terminate(1);
	}
    }
  
  /* Read header, do byte reversal, 
     if necessary, and check consistency */
  
  /* Lattice dimensions */
  
  if(psread_byteorder(byterevflag,parallel,fp,ksph->dims,sizeof(ksph->dims),
		   myname,"dimensions")!=0) terminate(1);

  if(ksph->dims[0] != nx || 
     ksph->dims[1] != ny ||
     ksph->dims[2] != nz ||
     ksph->dims[3] != nt)
    {
      /* So we can use this routine to discover the dimensions,
	 we provide that if nx = ny = nz = nt = -1 initially
	 we don't die */
      if(nx != -1 || ny != -1 || nz != -1 || nt != -1)
	{
	  printf("%s: Incorrect lattice dimensions ",myname);
	  for(j=0;j<4;j++)
	    printf("%d ",ksph->dims[j]); 
	  printf("\n"); fflush(stdout); terminate(1);
	}
      else
	{
	  nx = ksph->dims[0];
	  ny = ksph->dims[1];
	  nz = ksph->dims[2];
	  nt = ksph->dims[3];
	  volume = nx*ny*nz*nt;
	}
    }
  
  /* Date and time stamp */

  if(psread_data(parallel,fp,ksph->time_stamp,sizeof(ksph->time_stamp),
	      myname,"time stamp")!=0) terminate(1);

  /* Header byte length */

  ksph->header_bytes = sizeof(ksph->magic_number) + sizeof(ksph->dims) + 
    sizeof(ksph->time_stamp) + sizeof(ksph->order);
  
  /* Data order */
  
  if(psread_byteorder(byterevflag,parallel,fp,&ksph->order,sizeof(ksph->order),
		   myname,"order parameter")!=0) terminate(1);
  
  return byterevflag;
  
} /* read_ks_prop_hdr */

/*---------------------------------------------------------------------------*/

ks_prop_file *r_serial_ks_i(char *filename)
{
  /* Returns file descriptor for opened file */

  ks_prop_header *ksph;
  ks_prop_file *kspf;
  FILE *fp;
  int byterevflag;

  /* All nodes set up a propagator file and propagator header
     structure for reading */

  kspf = setup_input_ksprop_file(filename);
  ksph = kspf->header;

  /* File opened for serial reading */
  kspf->parallel = 0;

  /* Node 0 alone opens a file and reads the header */

  if(this_node==0)
    {
      fp = fopen(filename, "rb");
      if(fp == NULL)
	{
	  printf("r_serial_ks_i: Node %d can't open file %s, error %d\n",
		 this_node,filename,errno);fflush(stdout);terminate(1);
	}
      
/*      printf("Opened prop file %s for serial reading\n",filename); */
      
      kspf->fp = fp;

      byterevflag = read_ks_prop_hdr(kspf,SERIAL);

    }

  else kspf->fp = NULL;

  /* Broadcast the byterevflag from node 0 to all nodes */
      
  broadcast_bytes((char *)&byterevflag,sizeof(byterevflag));
  kspf->byterevflag = byterevflag;
  
  /* Node 0 broadcasts the header structure to all nodes */
  
  broadcast_bytes((char *)ksph,sizeof(ks_prop_header));

  /* Node 0 reads site list and assigns kspf->rank2rcv */

  /* No need for a read_site_list_w equivalent procedure here
     since only NATURAL_ORDER is presently supported */
  /** read_site_list_ks(SERIAL,kspf); **/
  /*  instead... */
  kspf->rank2rcv = NULL;

  return kspf;

}/* r_serial_ks_i */

/*----------------------------------------------------------------------*/

/* Here only node 0 reads the KS propagator from a binary file */

int r_serial_ks(ks_prop_file *kspf, int color, field_offset src)
{
  /* 0 is normal exit code
     1 for seek, read error, or missing data error */

  FILE *fp;
  ks_prop_header *ksph;
  char *filename;
  int byterevflag;

  off_t offset ;            /* File stream pointer */
  off_t ks_prop_size;       /* Size of propagator blocks for all nodes */
  off_t ks_prop_check_size; /* Size of propagator checksum record */
  off_t coord_list_size;    /* Size of coordinate list in bytes */
  off_t head_size;          /* Size of header plus coordinate list */
  off_t body_size ;         /* Size of propagator blocks for all nodes 
			      plus checksum record */
  int rcv_rank, rcv_coords;
  int destnode;
  int k,x,y,z,t;
  int status;
  int buf_length, where_in_buf;
  ks_prop_check test_kspc;
  u_int32type *val;
  int rank29,rank31;
  fsu3_vector *pbuf;
  su3_vector *dest;
  int idest;

  struct {
    fsu3_vector ksv;
    char pad[PAD_SEND_BUF];    /* Introduced because some switches
				  perform better if message lengths are longer */
  } msg;

  char myname[] = "r_serial_ks";

  fp = kspf->fp;
  ksph = kspf->header;
  filename = kspf->filename;
  byterevflag = kspf->byterevflag;

  status = 0;
  if(this_node == 0)
    {
      if(kspf->parallel == PARALLEL)
	printf("%s: Attempting serial read from parallel file \n",myname);
      
      ks_prop_size = volume*sizeof(fsu3_vector) ;
      ks_prop_check_size = sizeof(kspf->check.color) +
	sizeof(kspf->check.sum29) + sizeof(kspf->check.sum31);

      body_size = ks_prop_size + ks_prop_check_size;
    }
  broadcast_bytes((char *)&status,sizeof(int));
  if(status != 0) return status;

  status = 0;
  if(this_node == 0)
    {
      if(ksph->order == NATURAL_ORDER) coord_list_size = 0;
      else coord_list_size = sizeof(int32type)*volume;
      head_size = ksph->header_bytes + coord_list_size;
     
      offset = head_size + body_size*color;

      pbuf = (fsu3_vector *)malloc(MAX_BUF_LENGTH*sizeof(fsu3_vector));
      if(pbuf == NULL)
	{
	  printf("%s: Node %d can't malloc pbuf\n",myname,this_node);
	  fflush(stdout);
	  terminate(1);
	}
      
      /* Position file pointer for reading check record */

      if( fseeko(fp,offset,SEEK_SET) < 0 ) 
	{
	  printf("%s: Node %d fseeko %lld failed error %d file %s\n",
		 myname,this_node,(long long)offset,errno,filename);
	  fflush(stdout);
	  status = 1;
	}
      
      /* Read check record */

      status += sread_byteorder(byterevflag,fp,&kspf->check.color,
		      sizeof(kspf->check.color),myname,"check.color");
      status += sread_byteorder(byterevflag,fp,&kspf->check.sum29,
		      sizeof(kspf->check.sum29),myname,"check.sum29");
      status += sread_byteorder(byterevflag,fp,&kspf->check.sum31,
		      sizeof(kspf->check.sum31),myname,"check.sum31");

      /* Verify spin and color - checksums come later */
      if(kspf->check.color != color)
	{
	  printf("%s: color %d does not match check record on file %s\n",
		 myname,color,filename);
	  printf("  Check record said %d\n",kspf->check.color);
	  fflush(stdout); 
	  status = 1;
	}

      buf_length = 0;
      where_in_buf = 0;

    }
  
  broadcast_bytes((char *)&status,sizeof(int));
  if(status != 0) return status;

  /* all nodes initialize checksums */
  test_kspc.sum31 = 0;
  test_kspc.sum29 = 0;
  /* counts 32-bit words mod 29 and mod 31 in order of appearance
     on file */
  /* Here all nodes see the same sequence because we read serially */
  rank29 = 0;
  rank31 = 0;
  
  g_sync();

  /* Node 0 reads and deals out the values */
  status = 0;
  for(rcv_rank=0; rcv_rank<volume; rcv_rank++)
    {
      /* If file is in coordinate natural order, receiving coordinate
         is given by rank Otherwise, it is found in the table */
      
      if(kspf->header->order == NATURAL_ORDER)
	rcv_coords = rcv_rank;
      else
	rcv_coords = kspf->rank2rcv[rcv_rank];

      x = rcv_coords % nx;   rcv_coords /= nx;
      y = rcv_coords % ny;   rcv_coords /= ny;
      z = rcv_coords % nz;   rcv_coords /= nz;
      t = rcv_coords % nt;
      
      /* The node that gets the next su3_vector */
      destnode=node_number(x,y,z,t);

      if(this_node==0){
	/* Node 0 fills its buffer, if necessary */
	if(where_in_buf == buf_length)
	  {  /* get new buffer */
	    /* new buffer length  = remaining sites, but never bigger 
	       than MAX_BUF_LENGTH */
	    buf_length = volume - rcv_rank;
	    if(buf_length > MAX_BUF_LENGTH) buf_length = MAX_BUF_LENGTH;
	    /* then do read */
	    
	    if( (int)fread(pbuf,sizeof(fsu3_vector),buf_length,fp) 
		!= buf_length)
	      {
		if(status == 0)
		  printf("%s: node %d propagator read error %d file %s\n",
			 myname, this_node, errno, filename); 
		fflush(stdout); 
		status = 1;
	      }
	    where_in_buf = 0;  /* reset counter */
	  }  /*** end of the buffer read ****/

	/* Save vector in msg structure for further processing */
	msg.ksv = pbuf[where_in_buf];
	if(destnode==0){	/* just copy su3_vector */
	  idest = node_index(x,y,z,t);
	}
	else {		        /* send to correct node */
	  send_field((char *)&msg, sizeof(msg), destnode);
	}
	where_in_buf++;
      }

      /* The node that contains this site reads the message */
      else {	/* for all nodes other than node 0 */
	if(this_node==destnode){
	  idest = node_index(x,y,z,t);
	  /* Receive padded message in msg */
	  get_field((char *)&msg, sizeof(msg),0);
	}
      }

      /* The receiving node does the byte reversal and then checksum,
         if needed.  At this point msg.ksv contains the input vector
         and idest points to the destination site structure. */
      
      if(this_node==destnode)
	{
	  if(byterevflag==1)
	    byterevn((int32type *)(&msg.ksv),
		     sizeof(fsu3_vector)/sizeof(int32type));
	  /* Accumulate checksums */
	  for(k = 0, val = (u_int32type *)(&msg.ksv); 
	      k < (int)sizeof(fsu3_vector)/(int)sizeof(int32type); k++, val++)
	    {
	      test_kspc.sum29 ^= (*val)<<rank29 | (*val)>>(32-rank29);
	      test_kspc.sum31 ^= (*val)<<rank31 | (*val)>>(32-rank31);
	      rank29++; if(rank29 >= 29)rank29 = 0;
	      rank31++; if(rank31 >= 31)rank31 = 0;
	    }
	  /* Copy or convert vector from msg to lattice[idest] */
	  dest = (su3_vector *)F_PT( &(lattice[idest]), src );
	  f2d_vec(&msg.ksv, dest);
	}
      else
	{
	  rank29 += sizeof(fsu3_vector)/sizeof(int32type);
	  rank31 += sizeof(fsu3_vector)/sizeof(int32type);
	  rank29 %= 29;
	  rank31 %= 31;
	}
    }

  broadcast_bytes((char *)&status,sizeof(int));
  if(status != 0) return status;

  /* Combine node checksum contributions with global exclusive or */
  g_xor32(&test_kspc.sum29);
  g_xor32(&test_kspc.sum31);
  
  if(this_node==0)
    {
      printf("Read prop serially from file %s\n", filename);
      
      /* Verify checksum */
      /* Checksums not implemented until version 5 */

      if(ksph->magic_number == KSPROP_VERSION_NUMBER)
	{
	  if(kspf->check.sum29 != test_kspc.sum29 ||
	     kspf->check.sum31 != test_kspc.sum31)
	    {
	      printf("%s: Checksum violation color %d file %s\n",
		     myname, kspf->check.color, kspf->filename);
	      printf("Computed %x %x.  Read %x %x.\n",
		     test_kspc.sum29, test_kspc.sum31,
		     kspf->check.sum29, kspf->check.sum31);
	    }
/*	  else
	    printf("Checksums %x %x OK for file %s\n",
		   kspf->check.sum29,kspf->check.sum31,kspf->filename); */
	}
      fflush(stdout);
      free(pbuf);
    }

  return 0;

} /* r_serial_ks */

/*----------------------------------------------------------------------*/

void r_serial_ks_f(ks_prop_file *kspf)

/* Close the file and free associated structures */
{
  g_sync();
  if(this_node==0)
    {
      if(kspf->parallel == PARALLEL)
	printf("r_serial_w_f: Attempting serial close on parallel file \n");
      
      if(kspf->fp != NULL) fclose(kspf->fp);
/*      printf("Closed prop file %s\n",kspf->filename);*/
      fflush(stdout);
    }
  
  if(kspf->rank2rcv != NULL) free(kspf->rank2rcv);
  free(kspf->header);
  free(kspf);
  
} /* r_serial_ks_f */

/*---------------------------------------------------------------------------*/

/* ASCII file format

   format:
    version_number (int)
    time_stamp (char string enclosed in quotes)
    nx ny nz nt (int)
    for(t=...)for(z=...)for(y=...)for(x=...){
       for(i=...)for(j=...){prop[i][j].real, prop[i][j].imag}
    }
*/

/*---------------------------------------------------------------------------*/
/* Open and write header info for ascii propagator file */

ks_prop_file *w_ascii_ks_i(char *filename)
{
  ks_prop_header *ksph;
  ks_prop_file *kspf;
  FILE *fp;

  kspf = setup_output_ksprop_file();
  ksph = kspf->header;

  /* node 0 does all the writing */
  if(this_node==0){

    /* Set up ksprop file and ksprop header structures & load header values */

    fp = fopen(filename,"w");
    if(fp==NULL){
      printf("Can't open file %s, error %d\n",filename,errno); terminate(1);
    }

    kspf->fp = fp;

    if( (fprintf(fp,"%d\n", KSPROP_VERSION_NUMBER))==0 ){
      printf("Error in writing version number\n"); terminate(1);
    }
    if( (fprintf(fp,"\"%s\"\n",ksph->time_stamp))==0 ){
      printf("Error in writing time stamp\n"); terminate(1);
    }
    
    if( (fprintf(fp,"%d\t%d\t%d\t%d\n",nx,ny,nz,nt))==0 ){
      printf("Error in writing dimensions\n"); terminate(1);
    }

  }
  else kspf->fp = NULL;

  /* Assign remaining values to propagator file structure */
  kspf->parallel = 0;
  kspf->filename       = filename;
  kspf->rank2rcv       = NULL;         /* Not used for writing */
  kspf->byterevflag    = 0;            /* Not used for writing */

  /* Node 0 writes info file */
  if(this_node==0) write_ksprop_info_file(kspf);

  return kspf;

} /* w_ascii_ks_i */
  
/*---------------------------------------------------------------------------*/
/* Write ASCII propagator */

void w_ascii_ks(ks_prop_file *kspf, int color, field_offset src)
{
  FILE *fp;
  int currentnode,newnode;
  int b,l,x,y,z,t;
  fsu3_vector pbuf;
  int node0=0;

  g_sync();
  currentnode=0;

  fp = kspf->fp;
  
  /* first the color and spin */
  if(this_node==0)
    {
      if( (fprintf(fp,"%d\n",color))== EOF)
	{
	  printf("w_ascii_ks: error writing color\n"); 
	  terminate(1);
	} 
      fflush(fp);
    }

  /* next the elements */
  for(t=0;t<nt;t++)for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++)
    {
      newnode = node_number(x,y,z,t);
      if(newnode != currentnode)
	{	/* switch to another node */
	  g_sync();
	  /* Send a few bytes of garbage to tell newnode it's OK to send */
	  if( this_node==0 && newnode!=0 )send_field((char *)&pbuf,1,newnode);
	  if( this_node==newnode && newnode!=0 )get_field((char *)&pbuf,1,0);
	  currentnode=newnode;
	}
      
      if(this_node==0)
	{
	  if(currentnode==0)
	    {
	      l=node_index(x,y,z,t);
	      /* Copy, converting precision if necessary */
	      d2f_vec((su3_vector *)F_PT( &(lattice[l]), src ), &pbuf);
	    }
	  else
	    {
	      get_field((char *)&pbuf,sizeof(fsu3_vector),currentnode);
	    }
	  for(b=0;b<3;b++)
	    {
	      if( (fprintf(fp,"%.7e\t%.7e\n",(double)pbuf.c[b].real,
			   (double)pbuf.c[b].imag)) == EOF)
		{
		  printf("w_ascii_ks: error writing prop\n"); 
		  terminate(1);
		} 
	    }
	}
      else
	{	/* for nodes other than 0 */
	  if(this_node==currentnode)
	    {
	      l=node_index(x,y,z,t);
	      /* Copy, converting precision if necessary */
	      d2f_vec((su3_vector *)F_PT( &(lattice[l]), src ), &pbuf);
	      send_field((char *)&pbuf,sizeof(fsu3_vector),node0);
	    }
	}
    }
  g_sync();
  if(this_node==0)
    {
      fflush(fp);
      printf("Wrote prop to ASCII file  %s\n", kspf->filename);
    }

} /* w_ascii_ks */

/*---------------------------------------------------------------------------*/
/* Close ASCII propagator file */

void w_ascii_ks_f(ks_prop_file *kspf)
{
  g_sync();
  if(this_node==0)
    {
      fflush(kspf->fp);
      if(kspf->fp != NULL)fclose(kspf->fp);
      printf("Wrote ksprop file %s time stamp %s\n",kspf->filename,
	     (kspf->header)->time_stamp);
    }

  /* Free header and file structures */
  free(kspf->header);
  free(kspf);
}

/*---------------------------------------------------------------------------*/
/* Open ASCII propagator file and read header information */

ks_prop_file *r_ascii_ks_i(char *filename)
{
  ks_prop_file *kspf;
  ks_prop_header *ksph;
  FILE *fp;

  /* All nodes set up a propagator file and propagator header
     structure for reading */

  kspf = setup_input_ksprop_file(filename);
  ksph = kspf->header;

  /* File opened for serial reading */
  kspf->parallel = 0;
  kspf->byterevflag = 0;  /* Unused for ASCII */
  kspf->rank2rcv = NULL;  /* Unused for ASCII */

  /* Indicate coordinate natural ordering */
  ksph->order = NATURAL_ORDER;

  /* Node 0 alone opens a file and reads the header */

  if(this_node==0)
    {
      fp = fopen(filename,"r");
      if(fp==NULL)
	{
	  printf("r_ascii_ks_i: Node %d can't open file %s, error %d\n",
		 this_node,filename,errno); fflush(stdout); terminate(1);
        }
      kspf->fp = fp;

      if( (fscanf(fp,"%d",&ksph->magic_number))!=1 )
	{
	  printf("r_ascii_ks_i: Error in reading version number\n"); 
	  terminate(1);
	}
      if(ksph->magic_number != KSPROP_VERSION_NUMBER)
	{
	  printf("r_ascii_ks_i: Unrecognized magic number in propagator file header.\n");
	  printf("Expected %d but read %d\n",
		     KSPROP_VERSION_NUMBER, ksph->magic_number);
	  terminate(1);
	}
      if(fscanf(fp,"%*[ \f\n\r\t\v]%*[\"]%[^\"]%*[\"]",
		     ksph->time_stamp)!=1)
	{
	  printf("r_ascii_ks_i: Error reading time stamp\n"); 
	  terminate(1);
	}
      if( (fscanf(fp,"%d%d%d%d",&ksph->dims[0],&ksph->dims[1],
		  &ksph->dims[2],&ksph->dims[3]))!=4 )
	{
	  printf("r_ascii_ks_i: Error reading lattice dimensions\n"); 
	  terminate(1);
	}
      if( ksph->dims[0]!=nx || ksph->dims[1]!=ny 
	 || ksph->dims[2]!=nz || ksph->dims[3]!=nt )
	{
	  /* So we can use this routine to discover the dimensions,
	     we provide that if nx = ny = nz = nt = -1 initially
	     we don't die */
	  if(nx != -1 || ny != -1 || nz != -1 || nt != -1)
	    {
	      printf("r_ascii_ks_i: Incorrect lattice size %d,%d,%d,%d\n",
		     ksph->dims[0],ksph->dims[1],ksph->dims[2],ksph->dims[3]);
	      terminate(1);
	    }
	  else
	    {
	      nx = ksph->dims[0];
	      ny = ksph->dims[1];
	      nz = ksph->dims[2];
	      nt = ksph->dims[3];
	      volume = nx*ny*nz*nt;
	    }
	}
      ksph->header_bytes = 0;    /* Unused for ASCII */
    }

  else kspf->fp = NULL;  /* Other nodes don't know about this file */

  /* Broadcasts the header structure from node 0 to all nodes */
  
  broadcast_bytes((char *)ksph, sizeof(ks_prop_header));

  return kspf;

} /* r_ascii_ks_i */

/*---------------------------------------------------------------------------*/
/* Read a propagator */

int r_ascii_ks(ks_prop_file *kspf, int color, field_offset src)
{
  /* 0 normal exit code
     1 read error */

  ks_prop_header *ksph;
  FILE *fp;
  int destnode;
  int i,j,x,y,z,t;
  fsu3_vector pbuf;
  int status;

  ksph = kspf->header;
  fp = kspf->fp;

  g_sync();

  status = 0;
  if(this_node == 0)
    {
      if( (fscanf(fp,"%d",&j)) != 1 )
	{
	  printf("r_ascii_ks: Error reading color\n");
	  printf("r_ascii_ks: color=%d, j=%d\n",color,j);
	  status = 1;
	}
      if(status == 0 && (j != color))
	{
	  printf("r_ascii_ks: file file color=%d, prog color=%d\n", j, color);
	  status = 1;
	}
    }
  broadcast_bytes((char *)&status,sizeof(int));
  if(status != 0) return status;

  status = 0;
  for(t=0;t<nt;t++)for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++)
    {
      destnode=node_number(x,y,z,t);

      /* Node 0 reads, and sends site to correct node */
      if(this_node==0)
	{
	  for(j=0;j<3;j++)
	    {
	      if( (fscanf(fp,"%e%e\n",&(pbuf.c[j].real),
			  &(pbuf.c[j].imag)) )!= 2)
		{
		  if(status == 0)
		    printf("r_ascii_ks: Error reading su3_vector\n"); 
		  status = 1;
		}
	    }
	  if(destnode==0)
	    {              /* just copy su3_vector */
	      i = node_index(x,y,z,t);
	      /* Copy, converting precision if necessary */
	      f2d_vec(&pbuf, (su3_vector *)F_PT( &(lattice[i]), src ));
	    }
	  else 
	    {              /* send to correct node */
	      send_field((char *)&pbuf, sizeof(fsu3_vector), destnode);
	    }
	}
      
      /* The node which contains this site reads message */
      else
	{ 
	  /* for all nodes other than node 0 */
	  if(this_node==destnode)
	    {
	      get_field((char *)&pbuf, sizeof(fsu3_vector),0);
	      i = node_index(x,y,z,t);
	      /* Copy, converting precision if necessary */
	      f2d_vec(&pbuf, (su3_vector *)F_PT( &(lattice[i]), src ));
	    }
	}
    }

  broadcast_bytes((char *)&status, sizeof(int));

  return status;

} /* r_ascii_ks */

/*---------------------------------------------------------------------------*/
/* Close propagator file */

void r_ascii_ks_f(ks_prop_file *kspf)
{
  FILE *fp;

  fp = kspf->fp;

  g_sync();
  if(this_node==0)
    {
/*      printf("Closed ASCII prop file  %s\n", kspf->filename);*/
      fclose(fp);
      kspf->fp = NULL;
      fflush(stdout);
    }

} /* r_ascii_ks_f */

/*---------------------------------------------------------------------------*/
/* Quick and dirty code for binary output of propagator, separated
   into one file per timeslice */

void w_serial_ksprop_tt( char *filename, field_offset prop)
{

  char myname[] = "w_serial_ksprop_tt";
  FILE *fp;
  ks_prop_file *kspf;
  ks_prop_header *ksph;

  char tfilename[256];
  char *tag=".t";

  off_t offset;             /* File stream pointer */
  off_t ks_prop_size;        /* Size of propagator blocks for all nodes */
  off_t coord_list_size;    /* Size of coordinate list in bytes */
  off_t head_size;          /* Size of header plus coordinate list */
  off_t body_size ;         /* Size of propagator blocks for all nodes */
  int fseek_return;  /* added by S.G. for large file debugging */
  int currentnode, newnode;
  int x,y,z,t;
  register int i,a,b;
  fsu3_matrix pbuf;
  su3_vector *proppt;
  site *s;
    
  /* Set up ks_prop file and ks_prop header structs and load header values */
  kspf = setup_output_ksprop_file();
  ksph = kspf->header;
  ksph->order = NATURAL_ORDER;

  ks_prop_size = volume*sizeof(fsu3_matrix)/nt;
  body_size = ks_prop_size;

  /* No coordinate list was written because fields are to be written
     in standard coordinate list order */
  
  coord_list_size = 0;
  head_size = ksph->header_bytes + coord_list_size;
  
  /* OLD, forget the header now:  offset = head_size; */
  offset = 0;

  for(t=0; t<nt; t++) {
    
    /* Only node 0 opens the requested file */
    if(this_node == 0) {

      sprintf(tfilename, "%s%s%d", filename, tag, t);
      fp = fopen(tfilename, "wb");
      if(fp == NULL)
	{
	  printf("%s: Node %d can't open file %s, error %d\n",
		 myname,this_node,filename,errno); fflush(stdout);
	  terminate(1);
	}

      /* Node 0 writes the header */
      /* forget the header swrite_ks_prop_hdr(fp,ksph); */

    }

    /* Assign values to file structure */
    
    if(this_node==0) kspf->fp = fp; 
    else kspf->fp = NULL;             /* Only node 0 knows about this file */
    
    kspf->filename       = filename;
    kspf->byterevflag    = 0;            /* Not used for writing */
    kspf->rank2rcv       = NULL;         /* Not used for writing */
    kspf->parallel       = SERIAL;
    

    if(this_node==0) {

      fseek_return=fseeko(fp,offset,SEEK_SET);
      /* printf("w_serial_ksprop_t: Node %d fseek_return = %d\n",this_node,fseek_return); */
      if( fseek_return < 0 ) 
	{
	  printf("%s: Node %d fseeko %lld failed error %d file %s\n",
		 myname,this_node, (long long)offset, errno, kspf->filename);
	  fflush(stdout); terminate(1);
	}

    } /* end if(this_node==0) */

    /* Synchronize */  
    g_sync();
    
    /* Write propagator */
    currentnode = 0;

    for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++){
      newnode = node_number(x,y,z,t);
      if(newnode != currentnode){	/* switch to another node */
	/* Send a few bytes of garbage to tell newnode it's OK to send */
	if( this_node==0 && newnode!=0 )send_field((char *)&pbuf,1,newnode);
	if( this_node==newnode && newnode!=0 )get_field((char *)&pbuf,1,0);
	currentnode=newnode;
      }

      if(this_node==0){
	
	if(currentnode==0){ 
	  /* just copy */
	  i = node_index(x,y,z,t);
	  s = &(lattice[i]);
	  proppt = (su3_vector *)F_PT(s,prop);
	  /* Copy, converting precision if necessary */
	  for(a=0; a<3; a++){
	    for(b=0; b<3; b++){
	      pbuf.e[a][b].real = proppt[a].c[b].real;
	      pbuf.e[a][b].imag = proppt[a].c[b].imag;
	    }
	  }
	}
	else {
	  get_field((char *)&pbuf,sizeof(fsu3_matrix),currentnode);
	}
	
	/* write out */
	for(a=0; a<3; a++){
	  for(b=0; b<3; b++){
	    if( (int)fwrite(&(pbuf.e[a][b].real), sizeof(Real), 1, fp)
		!= 1 ) {
	      printf("w_serial_ksprop_tt: Node %d prop write error %d file %s\n",
		     this_node,errno,kspf->filename); 
	      fflush(stdout);
	      terminate(1);   
	    }
	    if( (int)fwrite(&(pbuf.e[a][b].imag), sizeof(Real), 1, fp)
		!= 1 ) {
	      printf("w_serial_ksprop_tt: Node %d prop write error %d file %s\n",
		     this_node,errno,kspf->filename); 
	      fflush(stdout);
	      terminate(1);   
	    }
	  }
	}

      }  else {    /* for nodes other than 0 */

	  if(this_node==currentnode){
	    i=node_index(x,y,z,t);
	    /* Copy data into send buffer and send to node 0 with padding */
	    s = &(lattice[i]);
	    proppt = (su3_vector *)F_PT(s,prop);
	    /* Copy, converting precision if necessary */
	    for(a=0; a<3; a++){
	      for(b=0; b<3; b++){
		pbuf.e[a][b].real = proppt[a].c[b].real;
		pbuf.e[a][b].imag = proppt[a].c[b].imag;
	      }
	    }
	    send_field((char *)&pbuf,sizeof(fsu3_matrix),0);
	  }
      }

    } /*close x,y,z loops */
  
    g_sync();
    if(this_node==0) fclose(fp);

  } /* end loop over t */

} /* end write_serial_ksprop_t */

/*---------------------------------------------------------------------------*/
/* Quick and dirty code for ascii output of propagator, separated
   into one file per timeslice */

void w_ascii_ksprop_tt( char *filename, field_offset prop) 
{

  char myname[] = "w_ascii_ksprop_tt";
  FILE *fp;
  ks_prop_file *kspf;
  ks_prop_header *ksph;

  char tfilename[256];
  char *tag=".t";
  int currentnode, newnode;
  int x,y,z,t;
  register int i,a,b;
  fsu3_matrix pbuf;
  su3_vector *proppt;
  site *s;

  /* Set up ks_prop file and ks_prop header structs and load header values */
  kspf = setup_output_ksprop_file();
  ksph = kspf->header;
  ksph->order = NATURAL_ORDER;

  for(t=0; t<nt; t++) {

    if(this_node == 0) {
      sprintf(tfilename, "%s%s%d", filename, tag, t);
      fp = fopen(tfilename, "w");
      if(fp == NULL){
	printf("%s: Node %d can't open file %s, error %d\n",
	       myname,this_node,filename,errno); fflush(stdout);
	terminate(1);
      }
    }

    /* Assign values to file structure */

    if(this_node==0) kspf->fp = fp; 
    else kspf->fp = NULL;             /* Only node 0 knows about this file */

    kspf->filename       = filename;
    kspf->byterevflag    = 0;            /* Not used for writing */
    kspf->rank2rcv       = NULL;         /* Not used for writing */
    kspf->parallel       = SERIAL;

    /* Synchronize */  
    g_sync();
    
    /* Write propagator */
    currentnode = 0;

    for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++){
      newnode = node_number(x,y,z,t);
      if(newnode != currentnode){	/* switch to another node */
	/**g_sync();**/
	/* tell newnode it's OK to send */
	if( this_node==0 && newnode!=0 )send_field((char *)&pbuf,1,newnode);
	if( this_node==newnode && newnode!=0 )get_field((char *)&pbuf,1,0);
	currentnode=newnode;
      }

      if(this_node==0){

	if(currentnode==0){ 
	  i = node_index(x,y,z,t);
	  s = &(lattice[i]);
	  proppt = (su3_vector *)F_PT(s,prop);

	  /* Copy, converting precision if necessary */
	  for(a=0; a<3; a++){
	    for(b=0; b<3; b++){
	      pbuf.e[a][b].real = proppt[a].c[b].real;
	      pbuf.e[a][b].imag = proppt[a].c[b].imag;
	    }
	  }
	}
	else {
	  get_field((char *)&pbuf,sizeof(fsu3_matrix),currentnode);
	}
	
	for(a=0;a<3;a++)for(b=0;b<3;b++){
	  if( (fprintf(fp,"%.7e\t%.7e\n",(double)pbuf.e[a][b].real,
		       (double)pbuf.e[a][b].imag))== EOF){
	    printf("Write error in save_ksprop_ascii\n"); terminate(1);
	  }
	}

      }

      else {	/* for all nodes other than node 0 */
	if(this_node==currentnode){
	  i = node_index(x,y,z,t);
	  s = &(lattice[i]);
	  proppt = (su3_vector *)F_PT(s,prop);
	  /* Copy, converting precision if necessary */
	  for(a=0; a<3; a++){
	    for(b=0; b<3; b++){
	      pbuf.e[a][b].real = proppt[a].c[b].real;
	      pbuf.e[a][b].imag = proppt[a].c[b].imag;
	    }
	  }
	  send_field((char *)&pbuf,sizeof(fsu3_matrix),0);
	} /* if */
      } /* else */

    } /* end loop over x,y,z */
  
    g_sync();
    if(this_node==0){
      fflush(fp);
      fclose(fp);
      fflush(stdout);
    }

  } /* end loop over t */

} /* end w_ascii_ksprop_tt */

/*********************************************************************/
/*********************************************************************/
/*  The following: restore_ksprop_ascii and save_ksprop_ascii are
    deprecated in favor of [w/r]_ascii_ks_i, [w/r]_ascii_ks, and
    [w/r]_ascii_ks_f
*/

/* Read a KS propagator in ASCII format serially (node 0 only) */

/* format:
    version_number (int)
    time_stamp (char string enclosed in quotes)
    nx ny nz nt (int)
    for(t=...)for(z=...)for(y=...)for(x=...){
       for(i=...)for(j=...){prop[i][j].real, prop[i][j].imag}
    }
*/

/* one su3_vector for each source color */
ks_prop_file *restore_ksprop_ascii( char *filename, field_offset prop )
{

  ks_prop_header *ph;
  ks_prop_file *pf;
  FILE *fp;
  int destnode;
  int version_number,i,a,b,x,y,z,t;
  fsu3_matrix pbuf;
  int src_clr;
  su3_vector *proppt;
  site *s;

  /* Set up a prop file and prop header structure for reading */

  pf = setup_input_ksprop_file(filename);
  ph = pf->header;

  /* File opened for serial reading */
  pf->parallel = 0;

  /* Node 0 opens the file and reads the header */

  if(this_node==0){
    fp = fopen(filename,"r");
    if(fp==NULL){
      printf("Can't open file %s, error %d\n",filename,errno);
      terminate(1);
    }

    pf->fp = fp;

    if( (fscanf(fp,"%d",&version_number))!=1 ){
      printf("restore_ksprop_ascii: Error reading version number\n"); 
      terminate(1);
    }
    ph->magic_number = version_number;
    if(ph->magic_number != KSPROP_VERSION_NUMBER_V0){
      printf("restore_ksprop_ascii: Incorrect version number in lattice header\n");
      printf("  read %d but expected %d\n",
	     ph->magic_number, KSPROP_VERSION_NUMBER_V0);
      terminate(1);
    }
    /* Time stamp is enclosed in quotes - discard the leading white
       space and the quotes and read the enclosed string */
    if((i = fscanf(fp,"%*[ \f\n\r\t\v]%*[\"]%[^\"]%*[\"]",ph->time_stamp))!=1){
      printf("restore_ksprop_ascii: Error reading time stamp\n"); 
      printf("count %d time_stamp %s\n",i,ph->time_stamp);
      terminate(1);
    }
    if( (fscanf(fp,"%d%d%d%d",&x,&y,&z,&t))!=4 ){
      printf("restore_ksprop_ascii: Error in reading dimensions\n"); 
      terminate(1);
    }

    ph->dims[0] = x; ph->dims[1] = y; ph->dims[2] = z; ph->dims[3] = t;
    if( ph->dims[0]!=nx || ph->dims[1]!=ny || 
       ph->dims[2]!=nz || ph->dims[3]!=nt )
      {
	/* So we can use this routine to discover the dimensions,
	   we provide that if nx = ny = nz = nt = -1 initially
	   we don't die */
	if(nx != -1 || ny != -1 || nz != -1 || nt != -1)
	  {
	    printf("restore_ksprop_ascii: Incorrect lattice size %d,%d,%d,%d\n",
		   ph->dims[0],ph->dims[1],ph->dims[2],ph->dims[3]);
	    terminate(1);
	  }
	else
	  {
	    nx = ph->dims[0];
	    ny = ph->dims[1];
	    nz = ph->dims[2];
	    nt = ph->dims[3];
	    volume = nx*ny*nz*nt;
	  }
      }

  } /* if node 0 */

  else pf->fp = NULL;

  /* Node 0 broadcasts the header structure to all nodes */
  
  broadcast_bytes((char *)ph,sizeof(ks_prop_header));

  /* Synchronize */  
  g_sync();
  
  for(t=0;t<nt;t++)for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++){
    destnode=node_number(x,y,z,t);
    
    /* Node 0 reads, and sends site to correct node */
    if(this_node==0){
      for(a=0;a<3;a++)for(b=0;b<3;b++){
	if( (fscanf(fp,"%e%e\n",&(pbuf.e[a][b].real),
		      &(pbuf.e[a][b].imag)) )!= 2){
	    printf("restore_ksprop_ascii: propagator read error\n"); 
	    terminate(1);
	}
      }

      if(destnode==0){	/* just copy */
	i = node_index(x,y,z,t);
	s = &(lattice[i]);
	proppt = (su3_vector *)F_PT(s,prop);
	/* Copy, converting precision if necessary */
	for(src_clr=0; src_clr<3; src_clr++){
	  for(b=0; b<3; b++){
	    proppt[src_clr].c[b].real = pbuf.e[src_clr][b].real;
	    proppt[src_clr].c[b].imag = pbuf.e[src_clr][b].imag;
	  }
	}
      }
      else {		/* send to correct node */
	send_field((char *)&pbuf,sizeof(fsu3_matrix),destnode);
      }
    }

    /* The node which contains this site reads message */
    else {	/* for all nodes other than node 0 */
      if(this_node==destnode){
	get_field((char *)&pbuf,sizeof(fsu3_matrix),0);
	i = node_index(x,y,z,t);
	s = &(lattice[i]);
	proppt = (su3_vector *)F_PT(s,prop);
	/* Copy, converting precision if necessary */
	for(src_clr=0; src_clr<3; src_clr++){
	  for(b=0; b<3; b++){
	    proppt[src_clr].c[b].real = pbuf.e[src_clr][b].real;
	    proppt[src_clr].c[b].imag = pbuf.e[src_clr][b].imag;
	  }
	}
      } /* if */
    } /* else */

  } /* end loop over x,y,z,t */
  
  g_sync();
  if(this_node==0){
    printf("Restored propagator from ascii file  %s\n",
	   filename);
    printf("Time stamp %s\n",ph->time_stamp);
    fclose(fp);
    pf->fp = NULL;
    fflush(stdout);
  }

  return pf;

} /* end restore_ksprop_ascii() */

/*---------------------------------------------------------------------*/

/* Save a KS propagator in ASCII format serially (node 0 only) */

/* one su3_vector for each source color */
ks_prop_file *save_ksprop_ascii(char *filename, field_offset prop)
{

  ks_prop_header *ph;
  ks_prop_file *pf;
  FILE *fp;
  int currentnode, newnode;
  int i,a,b,x,y,z,t;
  fsu3_matrix pbuf;
  int src_clr;
  su3_vector *proppt;
  site *s;

  /* Set up a prop file and prop header structure for reading */

  pf = setup_output_ksprop_file();
  ph = pf->header;

  /* node 0 does all the writing */

  if(this_node==0){
    fp = fopen(filename,"w");
    if(fp==NULL){
      printf("Can't open file %s, error %d\n",filename,errno);
      terminate(1);
    }

    pf->fp = fp;
    pf->parallel = 0;
    pf->filename = filename;
    
    if( (fprintf(fp,"%d\n",KSPROP_VERSION_NUMBER_V0))==0 ){
      printf("Error in writing version number\n"); terminate(1);
    }
    if( (fprintf(fp,"\"%s\"\n",ph->time_stamp))==0 ){
      printf("Error in writing time stamp\n"); terminate(1);
    }
    
    if( (fprintf(fp,"%d\t%d\t%d\t%d\n",nx,ny,nz,nt))==0 ){
      printf("Error in writing dimensions\n"); terminate(1);
    }

    write_ksprop_info_file(pf);

  } /* if node 0 */

  /* Synchronize */  
  g_sync();
  
  /* Write propagator */
  currentnode = 0;

  for(t=0;t<nt;t++)for(z=0;z<nz;z++)for(y=0;y<ny;y++)for(x=0;x<nx;x++){
    newnode = node_number(x,y,z,t);
    if(newnode != currentnode){	/* switch to another node */
      /**g_sync();**/
      /* tell newnode it's OK to send */
      if( this_node==0 && newnode!=0 )send_field((char *)&pbuf,1,newnode);
      if( this_node==newnode && newnode!=0 )get_field((char *)&pbuf,1,0);
      currentnode=newnode;
    }

    if(this_node==0){

      if(currentnode==0){ 
	i = node_index(x,y,z,t);
	s = &(lattice[i]);
	proppt = (su3_vector *)F_PT(s,prop);
	/* Copy, converting precision if necessary */
	for(src_clr=0; src_clr<3; src_clr++){
	  for(b=0; b<3; b++){
	    pbuf.e[src_clr][b].real = proppt[src_clr].c[b].real;
	    pbuf.e[src_clr][b].imag = proppt[src_clr].c[b].imag;
	  }
	}
      }
      else {
	get_field((char *)&pbuf,sizeof(fsu3_matrix),currentnode);
      }

      for(a=0;a<3;a++)for(b=0;b<3;b++){
	if( (fprintf(fp,"%.7e\t%.7e\n",(double)pbuf.e[a][b].real,
		     (double)pbuf.e[a][b].imag))== EOF){
	  printf("Write error in save_ksprop_ascii\n"); terminate(1);
	}
      }

    }

    else {	/* for all nodes other than node 0 */
      if(this_node==currentnode){
	i = node_index(x,y,z,t);
	s = &(lattice[i]);
	proppt = (su3_vector *)F_PT(s,prop);
	/* Copy, converting precision if necessary */
	for(src_clr=0; src_clr<3; src_clr++){
	  for(b=0; b<3; b++){
	    pbuf.e[src_clr][b].real = proppt[src_clr].c[b].real;
	    pbuf.e[src_clr][b].imag = proppt[src_clr].c[b].imag;
	  }
	}
	send_field((char *)&pbuf,sizeof(fsu3_matrix),0);
      } /* if */
    } /* else */

  } /* end loop over x,y,z,t */
  
  g_sync();
  if(this_node==0){
    fflush(fp);
    printf("Saved propagator to ascii file  %s\n",
	   filename);
    printf("Time stamp %s\n",ph->time_stamp);
    fclose(fp);
    fflush(stdout);
  }

  return pf;

} /* end save_ksprop_ascii() */

