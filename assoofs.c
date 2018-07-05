/*
*
*PRACTICA FINAL ASSOO
*FRANCISCO JAVIER ALVAREZ DE CELIS
*
*/

#include <linux/module.h>       
#include <linux/kernel.h>       
#include <linux/init.h>        
#include <linux/fs.h>           
#include <linux/buffer_head.h>  
#include <linux/slab.h>         
#include "assoofs.h"

/**
*
*/

static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);
static struct inode *assoofs_get_inode(struct super_block *sb , int ino);
static int assoofs_create(struct inode *dir,struct dentry *dentry, umode_t mode, bool excl);
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int assoofs_iterate(struct file *filp, struct dir_context *ctx);

static int assoofs_create_fs_object(struct inode *dir, struct dentry *dentry, umode_t mode);
static struct kmem_cache *assoofs_inode_cache;

int assoofs_fill_super(struct super_block *sb, void *data, int silent);
int assoofs_sb_get_a_freeblock(struct super_block *sb ,uint64_t *block);
void assoofs_save_sb_info (struct super_block *vsb);
void assoofs_add_inode_info(struct super_block *sb , struct assoofs_inode_info *inode);
int assoofs_save_inode_info(struct super_block *sb , struct assoofs_inode_info *inode_info);

void assoofs_destroy_inode(struct inode *inode);

ssize_t assoofs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos);
ssize_t assoofs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos);

struct assoofs_inode_info*assoofs_get_inode_info ( struct super_block * sb , uint64_t inode_no);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search);



static struct file_system_type assoofs_type = {
	.owner = THIS_MODULE,
 	.name = "assoofs",
 	.mount = assoofs_mount,
 	.kill_sb = kill_litter_super,
 };

 static const struct super_operations assoofs_sops = {
	.drop_inode = generic_delete_inode,
	
};

//ESTRUCTURA PARA MANEJAR INODOS
static struct inode_operations assoofs_inode_ops = {
	.lookup = assoofs_lookup,
	.create = assoofs_create,
	.mkdir = assoofs_mkdir,
};

//ESTRUCTURA PARA MANEJAR DIRECTORIOS
const struct file_operations assoofs_dir_operations = {
 	.owner = THIS_MODULE,
 	.iterate = assoofs_iterate,
 };

//ESTRUCTURA PARA MANEJAR FICHEROS
 const struct file_operations assoofs_file_operations = {
 	.read = assoofs_read,
 	.write = assoofs_write,
 };


 static int assoofs_create_fs_object(struct inode *dir, struct dentry *dentry, umode_t mode){

    struct buffer_head *bh;
    struct inode *inode;
    struct super_block *sb;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct assoofs_dir_record_entry *dir_contents;
    struct assoofs_super_block_info *sb_disk;
    uint64_t count;

    sb = dir->i_sb;
    sb_disk = sb->s_fs_info;
    count = sb_disk->inodes_count;

    if(count > ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED){

        return -1;
    }
    printk(KERN_INFO "CREATE\n");

    inode = new_inode(sb);
    inode->i_sb = sb;
    inode->i_op = &assoofs_inode_ops;
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    inode->i_ino = (count + ASSOOFS_START_INO - ASSOOFS_RESERVED_INODES + 1);

    inode_info = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);

    inode_info = assoofs_get_inode_info(sb, inode->i_ino);
    inode_info->inode_no = inode->i_ino;
    inode->i_private = inode_info;
    inode_info->mode = mode;


    if(S_ISDIR(mode)){
        printk(KERN_INFO "New directory creation request\n");
        inode_info->dir_children_count = 0;
        inode->i_fop = &assoofs_dir_operations;

    }else if(S_ISREG(mode)){
        printk(KERN_INFO "New file creation request\n");
        inode_info->file_size = 0;
        inode->i_fop = &assoofs_file_operations;
    }

    assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);

    assoofs_add_inode_info(sb, inode_info);

    parent_inode_info = dir->i_private;
    bh = sb_bread(sb, parent_inode_info->data_block_number);

    dir_contents = (struct assoofs_dir_record_entry *) bh->b_data;
    dir_contents += parent_inode_info->dir_children_count;
    dir_contents->inode_no = inode_info->inode_no;

    strcpy(dir_contents->filename, dentry->d_name.name);

    //save
    assoofs_save_inode_info(sb, parent_inode_info);

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    parent_inode_info->dir_children_count++;
    assoofs_save_inode_info(sb, parent_inode_info);

    inode_init_owner(inode, dir, mode);
    d_add(dentry, inode);
    return 0;
}


/*
*APARTADO 2.3.3
*/

 int assoofs_fill_super(struct super_block *sb, void *data, int silent){//FUNCION PARA INICIALIZAR EL SUPERBLOQUE

	struct inode *root_inode;
	struct buffer_head *bh;
	struct assoofs_super_block_info *assoofs_sb;
	
	//Leer la informacion persistente del superbloque del dispositivo de bloques
	bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
	assoofs_sb = (struct assoofs_super_block_info *)bh->b_data;


	//Comprobar que magic es correcto
	if (assoofs_sb->magic == ASSOOFS_MAGIC) {
		printk(KERN_INFO "Magic correcto");
		
	}else{
		printk(KERN_ERR "Magic incorrecto");
	}

	//comprobamos que el tamaño del bloque es correcto
	if (assoofs_sb->block_size == ASSOOFS_DEFAULT_BLOCK_SIZE) {
		printk(KERN_INFO"Tamanio bloque correcto");
		
	}else{
		printk(KERN_ERR"Tamanio bloque incorrecto");
	}
	

	
	sb->s_magic = ASSOOFS_MAGIC;
	sb->s_fs_info = assoofs_sb;
	sb->s_maxbytes = ASSOOFS_DEFAULT_BLOCK_SIZE;
	sb->s_op = &assoofs_sops;

	//CREACION DEL INODO RAIZ
	root_inode = new_inode(sb);
	root_inode->i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER;
	inode_init_owner(root_inode, NULL, S_IFDIR);
	root_inode->i_sb = sb;
	root_inode->i_op = &assoofs_inode_ops;
	root_inode->i_fop = &assoofs_dir_operations;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(root_inode);

	root_inode->i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER);

	

	//INTRODUCIR INODO EN EL ARBOL DE INODOS
	sb->s_root = d_make_root(root_inode);
	brelse(bh);

	return 0;//TODO CORRECTO
}
//Obtenemos la informacion persistente del inodo numero inode_no del superbloque
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb , uint64_t inode_no){

	//Accedemos al bloque que contiene el almacen de inodos
	struct assoofs_inode_info *inode_buffer;
	struct buffer_head *bh ;
	struct assoofs_super_block_info *afs_sb;
	struct assoofs_inode_info *inode_info;

	bh = sb_bread(sb , ASSOOFS_INODESTORE_BLOCK_NUMBER);
	inode_info = (struct assoofs_inode_info*)bh->b_data;

	//Recorremos el almacen en busca del inodo_no
	int i;
	for(i = 0; i < afs_sb->inodes_count; i++){
		if(inode_info->inode_no == inode_no){

			inode_buffer = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
			memcpy(inode_buffer, afs_sb, sizeof(*inode_buffer));
			break;
		}
		inode_info++;
	}

	//liberamos recursos y retornamos el inodo
	brelse(bh);
	return inode_buffer; //falla aqui 

	/**
	*APARTADO 2.3.4
	*/
}
//RECORRER EL ARBOL DE INODOS, BUSCA LA ENTRADA CON EL NOMBRE CORRECTO EN EL DIRECTORIO PADRE
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags){

	
	//ACEDEMOS AL BLOQUE DE DISCO CON EL CONTENIDO DEL DIRECTORIO APUNTADO POR PARENT_INODE
	struct super_block *sb;
	struct inode *inode;
	struct assoofs_dir_record_entry *record;
	struct buffer_head *bh;
	struct assoofs_inode_info *parent_info;
	struct assoofs_inode_info *inode_info;
	
	bh = sb_bread(sb, parent_info->data_block_number);
	
	//BUSCAMOS LA ENTRADA CUYO NOMBRE SEA EL QUE BUSCAMOS, SI SE ENCUENTRA, CONSTRUIMOS EL INODO CORRESPONDIENTE
	record = (struct assoofs_dir_record_entry *)bh->b_data;
	int i;
	for (i = 0; i < parent_info->dir_children_count; i++) {
		

		if (!strcmp(record->filename, child_dentry->d_name.name)) {
			

			struct inode *inode = assoofs_get_inode(sb, record->inode_no);
			inode_init_owner(inode, parent_inode, ((struct assoofs_inode_info *)inode->i_private)->mode);
			d_add(child_dentry, inode);
			return NULL;
		}
		record++;
	}

	
	return NULL;//SIEMPRE DEVOLVEMOS NULL
}

//OBTENEMOS UN PUNETERO AL INODO INO DE SB
static struct inode *assoofs_get_inode(struct super_block *sb , int ino ){

	//OBTENEMOS LA INFORMACION PERSISTENTE
	struct inode *inode;
	struct assoofs_inode_info *inode_info;
	inode_info = assoofs_get_inode_info(sb, ino);

	//CREAMOS LOS CAMPOS E INOCIALIZAMOS NEW_INODE
	if (S_ISDIR(inode_info->mode)){

		inode->i_fop = &assoofs_dir_operations;

	}else if (S_ISREG (inode_info-> mode)){

		inode->i_fop = &assoofs_file_operations;

	}else{

		printk(KERN_ERR "Unknown inode type. Neither a directory nor a file.");
	}//POSIBLE ERROR EN LAS LLAVES DE AQUI

	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode); //asignamos current_time a atime mtime ctime
	inode->i_private = inode_info;

	return inode;//devolvemos el inode creado

}

//CREAR INODOS PARA ARCHIVOS
static int assoofs_create(struct inode *dir,struct dentry *dentry, umode_t mode, bool excl){

	return assoofs_create_fs_object(dir, dentry, mode);//al ser muy similar a mkdir llamamos a create_fs_object
}

//FUNCION AUXILIAR QUE PERMITE OBTENER UN BLOQUE LIBRE
int assoofs_sb_get_a_freeblock(struct super_block *sb ,uint64_t *block){

	//OBTENEMOS LA INFORMACION PERSISTENTE DEL SUPERBLOQUE
	struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
	int i;
	
	//RECORREMOS EL AMPA DE BITS EN BUSCA DE UN BLOQUE LIBRE
	for (i = 2; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++) {
		if (assoofs_sb->free_blocks & (1 << i)) {
			break;
		}
	}


	*block = i;
	//GUARDAMOS LOS CAMBIOS DEL SUPERBLOQUE
	assoofs_sb->free_blocks &= ~(1 << i);

	assoofs_save_sb_info(sb);

	return 0;//TODO BIEN

}

//FUNCION AUXILIAR, NOS PERMITE ACTUALIZAR LA INFORMACION PERSISTENTE DEL SUPERBLOQUE CUANDO HAY UN CAMBIO
void assoofs_save_sb_info(struct super_block *vsb){

	//ACEDEMOS A LA INFORMACION PERSISTENTE DEL SUPERBLOQUE
	struct buffer_head *bh ;
	struct assoofs_super_block *sb = vsb->s_fs_info; // Información persistente del superbloque en memoria

	bh = sb_bread (vsb ,ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
	bh->b_data = (char *)sb; // Sobreescribo los datos de disco con la información en memoria

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);//MARCO EL BUFFER COMO SUCIO PARA QUE EL CAMBIO PASE AL DISCO
	brelse(bh);

}

//NOS PERMITIRA GUARDAR EN EL DISCO TODA LA INFORMACION PERSISTENTE DE UN NUEVO INODO
void assoofs_add_inode_info(struct super_block *sb , struct assoofs_inode_info *inode){

	//ACEDEMOS A LA INFORMACION PERSISTENTE DEL SUPERBLOQUE
	struct assoofs_super_block_info *assoofs_sb;
	struct buffer_head *bh;
	struct assoofs_inode_info *inode_info;

	
	//LEEMOS EL DISCO QUE TIENE EL ALMACEN DE INODOS
	bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
	
	//OBTENEMOS UN PUNTERO AL FUNAL DEL ALMACEN Y SE ESCRIBE OTRO VALOR FINAL
	inode_info = (struct assoofs_inode_info *)bh->b_data;
	inode_info += assoofs_sb->inodes_count;
	memcpy(inode_info, inode, sizeof(struct assoofs_inode_info));
	assoofs_sb->inodes_count++;//ACTUALIZAMOS EL CONTADOR DE INODOS

	mark_buffer_dirty(bh);//MARCAMOS COMO SUCIO Y SINCRONIZAMOS
	assoofs_save_sb_info(sb);//GUARDAMOS INFORMACION
	brelse(bh);
}

//NOS PERMITE ACTUALIZAR LA INFORMACION PERSISTENTE DE UN INODO
int assoofs_save_inode_info(struct super_block *sb , struct assoofs_inode_info *inode_info){

	//OBTENEMOS EL ALMACEN DE INODOS
	struct assoofs_inode_info *inode_pos;
	struct buffer_head *bh;

	bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
	//BUSCAMOS INODE_INFO
	inode_pos = assoofs_get_inode_info(sb, inode_info->inode_no);

	//ACTUALIZAMOS, MARCAMOS COMO SUCIO Y SINCRONIZAMOS
	memcpy(inode_pos,inode_info, sizeof(*inode_pos));
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	return 0;//TOO CORRECTO
}

//NOS PERMITIRA OBTENER UN PUNTERO A LA INFORMACION PERSISTENTE DE UN INODO EN CONCRETO
struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search){

    uint64_t count = 0;

    //RECCOREMOS EL ALAMCEN DE INODOS HASTA ENCONTRAR LOS DATOS DE SEARCH O HASTA EL FN¡INAL
    while(start->inode_no != search->inode_no && count < ((struct assoofs_super_block_info *) sb->s_fs_info)->inodes_count){

        count++;
        start++;
    }

    if(start->inode_no == search->inode_no){
      return start;
    }

	return NULL;
}

//NOS PERMITE CREAR NUEVOS INODOS PARA DIRECTORIOS
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode){

	return assoofs_create_fs_object(dir, dentry, S_IFDIR | mode);//al ser muy similar a create llamamos a create_fs_object
}

/**
*APARTADO 2.3.5
*
*/

//PERMITE MOSTRAR EL CONTENIDO DE UN DIRECTORIO(LS)
static int assoofs_iterate(struct file *filp, struct dir_context *ctx){

	//ACEDEMOS A LA INFORMACION PERSISTENTE DEL INIDO, Y EL SUPERBLOQUE CORRESPONDIENTE AL ARGUMENTO FILP
	struct inode * inode;
	struct buffer_head *bh;
	struct assoofs_inode_info *inode_info;
	struct assoofs_dir_record_entry *record;

	
	inode = filp->f_path.dentry->d_inode;
	inode_info = inode->i_private;

	//COMPROBAMOS SI EL DIRECTORIO YA ESTA CREADO
	if(ctx->pos)return 0;
	//COMPROBAR SI EL INODO ONTENIDO COORESPOONDE CON UN DIRECTORIO
	if ((!S_ISDIR(inode_info->mode)))return -1;

	//ACCEDEMOS AL BLOQUE DONDE SE ALMACENA EL CONTENIDO DEL DIRECTORIO Y CON LA INFORMACION QUE CONTIENE INICIALIZAMOS EL CONTEXTO CTX
	bh = sb_bread(inode->i_sb, inode_info->data_block_number);
	record = (struct assoofs_dir_record_entry *) bh->b_data;

	int i;
	for(i = 0; i < inode_info-> dir_children_count;i++) {

		dir_emit(ctx,record->filename,ASSOOFS_FILENAME_MAXLEN,record->inode_no,DT_UNKNOWN);
		ctx-> pos += sizeof(struct assoofs_dir_record_entry);//CADA VEZ QUE SE AÑADE UNA ENTRADA AL CONTEXTO INCREMENTAMOS POS
		record ++;
	}

	brelse ( bh ) ;
	return 0;

}

//PERMITE LEER ARCHIVOS
ssize_t assoofs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos){



	struct inode * inode;
	struct super_block *sb;
	struct buffer_head *bh;
	struct assoofs_inode_info *inode_info;
	
	char *buffer;
	int nbytes ;

	//ACEDEMOS LA CONTENIDO DEL FICHERO
	if (*ppos >= inode_info->file_size) return 0;

	bh = sb_bread(filp->f_path.dentry->d_inode->i_sb,inode_info->data_block_number);

	if (!bh) {
		printk(KERN_ERR "Reading the block number [%llu] failed.",
		       inode_info->data_block_number);
		return 0;
	}

	buffer = (char *)bh->b_data;

	//COPIAMOS EN EL BUFFER EL CONTENIDO DEL FICHERO LEIDO
	nbytes = min((size_t)inode_info->file_size,len); 
	//ONCREMENTAMOS PPOS Y DEVILVEMOS LOS BYTES LEIDOS
	copy_to_user(buf,buffer,nbytes);
	*ppos += nbytes;
	return nbytes;

}

//NOS PERMITE ESCRIBIR UN ARCHIVO
ssize_t assoofs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos){

	struct inode * inode;
	struct super_block *sb;
	struct buffer_head *bh;
	struct assoofs_inode_info *inode_info;
	struct assoofsfs_dir_record_entry *record;

	char *buffer;

    printk(KERN_INFO "WRITE\n");

    sb = filp->f_path.dentry->d_inode->i_sb;
    inode = filp->f_path.dentry->d_inode;
    inode_info = inode->i_private;

	bh = sb_bread(sb, inode_info->data_block_number);
	if (!bh) {
		printk(KERN_ERR "Reading the block number [%llu] failed.\n", inode_info->data_block_number);
		return 0;
	}

    //if(*ppos >= inode_info->file_size) return 0;

    buffer = (char *)bh->b_data;
    buffer += *ppos;

    if (copy_from_user(buffer, buf, len)) {
		brelse(bh);
		printk(KERN_ERR "Error copying from user\n");
		return 0;
	}
    *ppos += len;
    inode_info->file_size = *ppos;

   	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
    brelse(bh);

    assoofs_save_inode_info(sb, inode_info);

	return len;
}

//MONTAR DISCO
static struct dentry *assoofs_mount(struct file_system_type *fs_type,int flags, const char *dev_name,void *data){

	struct dentry *ret;

	ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);

	if (unlikely(IS_ERR(ret))){
		printk(KERN_ERR "Error mounting assoofs");
	}else{
		printk(KERN_INFO "assoofs is succesfully mounted on [%s]\n",
		       dev_name);
	}

	return ret;
}

static int assoofs_init(void){ //ASSOOFS INIT

	assoofs_inode_cache = kmem_cache_create("assoofs _inode_cache",sizeof(struct assoofs_inode_info),0,(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),NULL);
	
	if (register_filesystem(&assoofs_type)){
		printk(KERN_INFO "Sucessfully registered assoofs\n");
	}else{
		printk(KERN_ERR "Failed to register assoofs");
		return -1;
	}
}

static void assoofs_exit(void){ //ASSOOFS EXIT

	if (unregister_filesystem(&assoofs_type)){
		printk(KERN_INFO "Sucessfully unregistered assoofs\n");
	}else{
		printk(KERN_ERR "Failed to unregister assoofs");
	}
}


module_init(assoofs_init);
module_exit(assoofs_exit);
