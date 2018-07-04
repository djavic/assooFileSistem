#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
//<#include <asm/uaccess.h>        /* copy_to_user          */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francisco Javier Alvarez de Celis");

/**
*
*/
//MONTAR NUEVO SISTEMA DE FICHEROS
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);
static struct inode *assoofs_get_inode(struct super_block *sb , int ino );
static int assoofs_create(struct inode *dir,struct dentry *dentry, umode_t mode, bool excl);
int assoofs_fill_super(struct super_block *sb, void *data, int silent);
int assoofs_sb_get_a_freeblock(struct super_block *sb ,uint64_t *block);
void assoofs_save_sb_info (struct super_block *vsb);
void assoofs_add_inode_info(struct super_block *sb , struct assoofs_inode_info *inode);
int assoofs_save_inode_info(struct super_block *sb , struct assoofs_inode_info *inode_info);



struct assoofs_inode_info*assoofs_get_inode_info ( struct super_block * sb , uint64_t inode_no);
struct dentry *simplefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
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

static struct inode_ope rationsassoofs_inode_ops = {
	.lookup = assoofs_lookup,
	.create = assoofs_create,
	.mkdir = assoofs_mkdir,
};


/*
*
*/

 int assoofs_fill_super(struct super_block *sb, void *data, int silent){//FUNCION PARA INICIALIZAR EL SUPERBLOQUE

	struct inode *root_inode;
	struct buffer_head *bh;
	struct assoofs_super_block *assoofs_disk;
	int ret = -EPERM;

	bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
	assoofs_sb = (struct assoofs_super_block *)bh->b_data;

	printk(KERN_INFO "The magic number obtained in disk is: [%llu]\n",
	       sb_disk->magic);

	if (assoofs_sb->magic == ASSOOFS_MAGIC) {
		printk(KERN_INFO "Magic correcto");
		
	}else{
		printk(KERN_ERR "Magic incorrecto");
	}


	if (assoofs_sb->block_size == ASSOOFS_DEFAULT_BLOCK_SIZE) {
		printk(KERN_INFO"Tamanio bloque correcto");
		
	}else{
		printk(KERN_ERR"Tamanio bloque incorrecto");
	}
	
	sb_disk->journal = NULL;

	printk(KERN_INFO
	       "simplefs filesystem of version [%llu] formatted with a block size of [%llu] detected in the device.\n",
	       sb_disk->version, sb_disk->block_size);

	
	sb->s_magic = ASSOOFS_MAGIC;
	sb->s_fs_info = assoofs_sb;
	sb->s_maxbytes = SIMPLEFS_DEFAULT_BLOCK_SIZE;
	sb->s_op = &assoofs_sops;
	//CREACION DEL INODO RAIZ
	root_inode = new_inode(sb);
	root_inode->i_ino = SIMPLEFS_ROOTDIR_INODE_NUMBER;
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
//INODEINFO
struct assoofs_inode_info*assoofs_get_inode_info ( struct super_block * sb , uint64_t inode_no){

	struct assoofs_inode_info*inode_info = NULL;
	struct buffer_head * bh ;


	bh = sb_bread ( sb , ASSOOFS_INODESTORE_BLOCK_NUMBER);
	inode_info = ( struct assoofs_inode_info*)bh -> b_data;

	struct assoofs_super_block_info *afs_sb = ab ->s_fs_info:
	struct assoofs_inode_info *buffer = NULL;

	int i
	for(i = 0; i < afs_sb->inodes_count; i++){
		if(inode_info->inode_no == inode_no){

			inode_buffer = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
			memcpy(inode_buffer, afs_sb, sizeof(*inode_buffer));
			break;
		}
		inode_info++;
	}

	brelse(bh);
	return buffer;

//RECORRER EL ARBOL DE INODOS
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags){

	struct assoofs_inode_info *parent_info = parent_inode->i_private;
	struct super_block *sb = parent_inode->i_sb;
	struct buffer_head *bh;
	struct assoofs_dir_record_entry *record;
	
	bh = sb_bread(sb, parent->data_block_number);
	

	record = (struct assofs_dir_record *)bh->b_data;
	int i;
	for (i = 0; i < parent->dir_children_count; i++) {
		

		if (!strcmp(record->filename, child_dentry->d_name.name)) {
			

			struct inode *inode = assoofs_get_inode(sb, record->inode_no);
			inode_init_owner(inode, parent_inode, ((struct assoofs_inode_info *)inode->i_private)->mode);
			d_add(child_dentry, inode);
			return NULL;
		}
		record++;
	}

	
	return NULL;
}

static struct inode *assoofs_get_inode(struct super_block *sb , int ino ){

	struct inode * inode ;
	inode_info = assoofs_get_inode_info(sb, ino);

	if (S_ISDIR(inode_info->mode))

		inode->i_fop = &assoofs_dir_operations;

	else if (S_ISREG (inode_info-> mode)){

		inode->i_fop = &assoofs_file_operations;

	else

		printk(KERN_ERR "Unknown inode type. Neither a directory nor a file.");
	

	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode); //asignamos current_time a atime mtime ctime
	inode->i_private = inode_info;

	return inode;

}

//CREAR INODOS PARA ARCHIVOS
static int assoofs_create(struct inode *dir,struct dentry *dentry, umode_t mode, bool excl){

	return assoofs_create_fs_object(dir, dentry, mode);
}

//FUNCION AUXILIAR QUE PERMITE OBTENER UN BLOQUE LIBRE
int assoofs_sb_get_a_freeblock(struct super_block *sb ,uint64_t *block){

	struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
	int i;
	
	
	for (i = 2; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++) {
		if (assoofs_sb->free_blocks & (1 << i)) {
			break;
		}
	}


	*block = i;

	assoofs_sb->free_blocks &= ~(1 << i);

	assoofs_sb_sync(sb);

	return 0;

}

//FUNCION AUXILIAR, NOS PERMITE ACTUALIZAR LA INFORMACION PERSISTENTE DEL SUPERBLOQUE CUANDO HAY UN CAMBIO
void assoofs_save_sb_info(struct super_block *vsb){

	struct buffer_head *bh ;
	struct assoofs_super_block *sb = vsb->s_fs_info ; // Informaci ó n persistente del superbloque en memoria

	bh = sb_bread (vsb ,ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
	bh->b_data = (char*)sb; // Sobreescribo los datos de disco con la informaci ó n en memoria

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

}

//NOS PERMITIRA GUARDAR EN EL DISCO TODA LA INFORMACION PERSISTENTE DE UN NUEVO INODO
void assoofs_add_inode_info(struct super_block *sb , struct assoofs_inode_info *inode){

	struct simplefs_super_block *assoofs_sb;
	struct buffer_head *bh;
	struct simplefs_inode *inode_info;

	

	bh = sb_bread(vsb, SIMPLEFS_INODESTORE_BLOCK_NUMBER);
	

	inode_info = (struct assoofs_inode_info *)bh->b_data;

	
	inode_info += sb->inodes_count;

	memcpy(inode_info, inode, sizeof(struct assoofs_inode));
	assoo_sb->inodes_count++;

	mark_buffer_dirty(bh);
	assoofs_save_sb_info(sb);
	brelse(bh);



int assoofs_save_inode_info(struct super_block *sb , struct assoofs_inode_info *inode_info){


	struct assoofs_inode_info *inode_pos;
	struct buffer_head *bh;

	bh = sb_bread(sb, SIMPLEFS_INODESTORE_BLOCK_NUMBER);
	inode_pos = simplefs_inode_search(sb,(struct assoofs_inode_info *)bh->b_data,inode_info);

	memcpy(inode_pos,inode_info, sizeof(*inode_pos));
	
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);

	return 0;
}

struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search){

    uint64_t count = 0;

    while(start->inode_no != search->inode_no && count < ((struct assoofs_super_block_info *) sb->s_fs_info)->inodes_count){

        count++;
        start++;
    }

    if(start->inode_no == search->inode_no){
      return start;
    }

	return NULL;
}















//MONTAR DISCO
static struct dentry *assoofs_mount(struct file_system_type *fs_type,int flags, const char *dev_name,void *data){

	struct dentry *ret;

	ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);

	if (unlikely(IS_ERR(ret))){
		printk(KERN_ERR "Error mounting simplefs");
	}else{
		printk(KERN_INFO "simplefs is succesfully mounted on [%s]\n",
		       dev_name);
	}

	return ret;
}

static int assoofs_init(void){ //ASSOOFS INIT

	

	assoofs_inode_cachep = kmem_cache_create("assoofs _inode_cache",sizeof(struct simplefs_inode),0,(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),NULL);
	

	
	if (register_filesystem(&assoofs_type)){
		printk(KERN_INFO "Sucessfully registered simplefs\n");
	}else{
		printk(KERN_ERR "Failed to register simplefs. Error:[%d]", ret);
	}

	return ret;
}

static void assoofs_exit(void){ //ASSOOFS EXIT

	

	
	if (unregister_filesystem(&assoofs_type)){
		printk(KERN_INFO "Sucessfully unregistered simplefs\n");
	}else{
		printk(KERN_ERR "Failed to unregister simplefs. Error:[%d]", ret);
	}
}


module_init(assoofs_init);
module_exit(assoofs_exit);