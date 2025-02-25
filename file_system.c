/* Requirements 

A. Sistema de Arquivos

- Sistema de arquivos dentro de um arquivo de 1 GB
- Diretório raiz com arquivos: implementar sem árvore de diretórios

B. Comandos (Criar, excluir, listar e armazenar)
1. criar(nome, tamanho)
    - Criar arquivo 'nome' com inteiros 32-bits aleatórios
    - Pode ser armazenado como binário ou string (binário parece mais eficiente). Obrigatório ser legível
    - Gerar 'tam' números
 
2. apagar(nome)
    - Apaga arquivo com nome 'nome'

3. listar
    - Mostrar todos os arquivos com seus tamanhos, espaço total consumido e espaço livre do sistema de arquivos

4. ordenar(nome): IMPLEMENTAR POR ULTIMO
    - Ordenar os inteiros no arquivo 'nome'. Método de ordenação é livre (usar algo já implementado)
    - Problema é quando o arquivo passar de 2 MB. 

5. ler(nome, inicio, fim)
    - Exibir sublista de um arquivo com nome passado como argumento. O intervalo da lista vai de 'inicio' a 'fim'

6. concatenar(nome1, nome2)
    - Concatena arquivos nome1 e nome2, cria um novo arquivo (ou sobrescreve nome1), e deleta os originais


Now, structuring the code. The main components would be:

- The virtual disk file (1GB) with structures for superblock, inodes, data blocks, and swap area.

- Functions to read/write inodes, data blocks, manage free space.

- Commands handling each operation (criar, apagar, etc.).

- Sorting with the huge page and paging to the swap area.

Possible data structures:

Superblock:

- Magic number to identify the file system.

- Total blocks.

- Free blocks count.

- Inode count.

- Root directory inode.

Inode structure:

- File name (fixed size, say 255 chars).

- File size in bytes.

- Block pointers (direct, maybe some indirect for larger files).

- Timestamps.

Each data block could be 4KB, similar to traditional file systems.

For the 1GB file layout:

- First block: superblock.

- Inode table: fixed number of inodes, maybe 1024.

- Data blocks: the rest of the space after superblock and inodes.

- Swap area: possibly a fixed section at the end of the file for paging during sorting.

So the 1GB file is divided into:

- Superblock (first block)

- Inode table

- Data blocks (for files)

- Swap area (fixed size, maybe 256MB)

Now, the code structure.

First, the virtual disk file:

- The code will create a 1GB file, initialized with the superblock, inode table, data blocks, and swap area.

- Need functions to read and write blocks from the virtual disk.

The in-memory data structures:

- A list of inodes, each representing a file.

- Free block management, perhaps using a bitmap.

But how to represent the inodes and free blocks in the virtual disk.

For simplicity, the superblock could contain:

- Magic number.

- Total blocks in the file system.

- Number of free blocks.

- Block size (e.g., 4096 bytes).

- Inode count.

- Inode table block number.

- Data block start.

- Swap area start.

The inode table is a contiguous block of inodes. Each inode has:

- File name (fixed length, say 255 bytes).

- File size.

- Block pointers (direct and indirect).

- Time stamps.

So the virtual disk layout:

Block 0: superblock.

Blocks 1 to N: inode table.

Blocks N+1 to M: data blocks (managed by the file system's free list).

Blocks M+1 to P: swap area.

Now, coding in C.

First, define the structures:

Superblock:

struct superblock {

uint32_t magic;

uint32_t block_size;

uint32_t fs_blocks; // number of blocks in the file system area (excluding swap)

uint32_t swap_blocks; // number of blocks in swap area

uint32_t inode_table_block; // starting block of inode table

uint32_t data_blocks_start; // starting block of data area

uint32_t swap_start; // starting block of swap area

uint32_t free_block_count;

uint32_t first_free_block; // block number of first free block in the data area

// ... other fields as needed

};

Inode:

struct inode {

char name[256];

uint32_t size; // in bytes

uint32_t first_block;

time_t created;

time_t modified;

// ... other fields

};

Each data block header (for linked list):

struct data_block {

uint32_t next_block; // 0 if last block

char data[block_size - sizeof(uint32_t)];

};

*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#define DISK_SIZE (1ULL * 1024 * 1024 * 1024) // 1 GB
#define HUGE_PAGE_SIZE (2 * 1024 * 1024) // 2 MB
#define MAX_FILENAME_LENGTH 255
#define MAX_FILES 1000
#define BLOCK_SIZE 4096       // Tamanho de um bloco (4 KB)
#define NUM_BLOCKS (DISK_SIZE / BLOCK_SIZE) // Número total de blocos

typedef struct {
    char nome[MAX_FILENAME_LENGTH]; 
    size_t tamanho;
    size_t posicao; 
} Arquivo;

// typedef struct {
//     unsigned char bitmap[NUM_BLOCKS / 8]; // 1 bit por bloco (array de bytes)
//     Arquivo arquivos[MAX_FILES];
//     size_t quantidade_arquivos;
//     size_t espaco_livre;
// } SistemaDeArquivos;

typedef struct {
    Arquivo arquivos[MAX_FILES];
    size_t quantidade_arquivos;
    size_t espaco_livre;
} SistemaDeArquivos;

SistemaDeArquivos sa;
FILE* disco_virtual;
// void* huge_page = NULL;

// Inicialização
void iniciar_sistema_arquivos() {
    disco_virtual = fopen("disco_virtual.bin", "r+b");
    if (!disco_virtual) {
        disco_virtual = fopen("disco_virtual.bin", "w+b");
        fseek(disco_virtual, DISK_SIZE - 1, SEEK_SET);
        fputc('\0', disco_virtual);
        fclose(disco_virtual);
        disco_virtual = fopen("disco_virtual.bin", "r+b");
    }
    sa.quantidade_arquivos = 0;
    sa.espaco_livre = DISK_SIZE;
}

// Comandos
// Criar
void criar(const char* nome, int tamanho) {
    if (sa.quantidade_arquivos >= MAX_FILES) {
        printf("Erro: Número máximo de arquivos atingido\n");
        return;
    }
    
    if (strlen(nome) >= MAX_FILENAME_LENGTH) {
        printf("Erro: Nome do arquivo é muito grande\n");
        return;
    }
    
    size_t file_size = tamanho * sizeof(int);
    if (file_size > sa.espaco_livre) {
        printf("Erro: Sem espaço suficiente\n");
        return;
    }
    
    Arquivo* arquivo = &sa.arquivos[sa.quantidade_arquivos++];
    strcpy(arquivo->nome, nome);
    arquivo->tamanho = file_size;
    arquivo->posicao = DISK_SIZE - sa.espaco_livre;
    sa.espaco_livre -= file_size;
    

    // Criar e armazenar números aleatórios no arquivo
    int* numbers = malloc(file_size); // Permitido usar
    if (!numbers) {
        printf("Erro: Falha ao alocar memória para os números\n");
        return;
    }

    for (int i = 0; i < tamanho; i++) {
        numbers[i] = rand() % 1000000; // Números aleatórios entre 0 e 999999
    }

    // Posicionar ponteiro do arquivo na posição correta e escrever os dados
    fseek(disco_virtual, arquivo->posicao, SEEK_SET);
    fwrite(numbers, sizeof(int), tamanho, disco_virtual);
    fflush(disco_virtual);  // Garante que os dados são gravados imediatamente

    free(numbers); // Liberar memória após a gravação

    printf("Arquivo '%s' criado com sucesso\n", nome);
}


// Listar
void listar() {
    printf("Arquivos:\n");
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        printf("%s - %zu bytes\n", sa.arquivos[i].nome, sa.arquivos[i].tamanho);
    }
    printf("\nEspaço total: %llu bytes\n", DISK_SIZE);
    printf("Espaço livre: %zu bytes\n", sa.espaco_livre);
}


// Ler
void ler(const char* nome, int inicio, int fim) {
    Arquivo* arquivo = NULL;
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, nome) == 0) {
            arquivo = &sa.arquivos[i];
            break;
        }
    }
    
    if (arquivo == NULL) {
        printf("Error: Arquivo '%s' não encontrado\n", nome);
        return;
    }

    int num_count = arquivo->tamanho / sizeof(int);
    
    if (inicio < 0 || fim >= num_count || inicio > fim) {
        printf("Error: Invalid range\n");
        return;
    }

    int* buffer = malloc(arquivo->tamanho);
    if (!buffer) {
        printf("Erro: Falha ao alocar memória\n");
        return;
    }

    fseek(disco_virtual, arquivo->posicao, SEEK_SET);
    fread(buffer, sizeof(int), num_count, disco_virtual);

    printf("Números %d a %d no arquivo '%s':\n", inicio, fim, nome);
    for (int i = inicio; i <= fim; i++) {
        printf("%d ", buffer[i]);
    }
    printf("\n");

  // Liberar memória
  free(buffer);
}


// apagar
void apagar(const char* nome) {
    int indice = -1;

    // Procurar pelo arquivo na lista
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, nome) == 0) {
            indice = i;
            break;
        }
    }

    // Se não encontrou, retorna erro
    if (indice == -1) {
        printf("Erro: Arquivo '%s' não encontrado\n", nome);
        return;
    }

    Arquivo* arquivo = &sa.arquivos[indice];

    // Limpar o espaço do arquivo no disco
    char* buffer = calloc(1, arquivo->tamanho);  // Buffer zerado
    if (!buffer) {
        printf("Erro: Falha ao alocar memória para apagar arquivo\n");
        return;
    }

    fseek(disco_virtual, arquivo->posicao, SEEK_SET);
    fwrite(buffer, 1, arquivo->tamanho, disco_virtual);
    fflush(disco_virtual);  // Garante que os dados são gravados
    free(buffer);  // Libera a memória

    // Atualizar espaço livre
    sa.espaco_livre += arquivo->tamanho;

    // Remover o arquivo da lista de arquivos
    for (int j = indice; j < sa.quantidade_arquivos - 1; j++) {
        sa.arquivos[j] = sa.arquivos[j + 1];
    }

    sa.quantidade_arquivos--;

    printf("Arquivo '%s' excluído com sucesso\n", nome);
}


// Concatenar
void concatenar(const char* nome1, const char* nome2) {
    Arquivo* arquivo1 = NULL;
    Arquivo* arquivo2 = NULL;
    
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, nome1) == 0) arquivo1 = &sa.arquivos[i];
        if (strcmp(sa.arquivos[i].nome, nome2) == 0) arquivo2 = &sa.arquivos[i];
    }
    
    if (arquivo1 == NULL || arquivo2 == NULL) {
        printf("Erro: Um dos arquivos não foi encontrado\n");
        return;
    }
    
    size_t novo_tamanho = arquivo1->tamanho + arquivo2->tamanho;
    if (novo_tamanho > sa.espaco_livre + arquivo1->tamanho + arquivo2->tamanho) {
        printf("Erro: Não há espaço suficiente para a concatenação\n");
        return;
    }
    
    // Criar buffer para armazenar conteúdo de arquivo2
    char* buffer = malloc(arquivo2->tamanho);
    if (!buffer) {
        printf("Erro: Falha ao alocar memória\n");
        return;
    }

    // Ler conteúdo de arquivo2
    fseek(disco_virtual, arquivo2->posicao, SEEK_SET);
    fread(buffer, 1, arquivo2->tamanho, disco_virtual);

    // Escrever conteúdo de arquivo2 no final de arquivo1
    fseek(disco_virtual, arquivo1->posicao + arquivo1->tamanho, SEEK_SET);
    fwrite(buffer, 1, arquivo2->tamanho, disco_virtual);
    fflush(disco_virtual);  // Garantir que os dados foram gravados
    
    sa.espaco_livre += arquivo1->tamanho;
    arquivo1->tamanho = novo_tamanho;

    free(buffer);
    
    printf("Arquivos '%s' e '%s' foram concatenados com sucesso\n", nome1, nome2);

    apagar(nome2);

    sa.espaco_livre -= novo_tamanho;
}


// Ordenar: implementar por último
void ordenar(const char* nome) {
    printf("ORDENAR\n");
};


// void allocate_huge_page() {
//     huge_page = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE,
//                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
//     if (huge_page == MAP_FAILED) {
//         fprintf(stderr, "Failed to allocate huge page\n");
//         exit(1);
//     }
// }

// void allocate_huge_page() {
//     int fd = open("/dev/hugepages/myfile", O_CREAT | O_RDWR, 0666);
//     if (fd < 0) {
//         perror("Failed to open huge page file");
//         exit(1);
//     }

//     huge_page = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE,
//                      MAP_PRIVATE | MAP_HUGETLB, fd, 0);
    
//     if (huge_page == MAP_FAILED) {
//         perror("Failed to allocate huge page");
//         close(fd);
//         exit(1);
//     }

//     close(fd);
// }

void* allocate_huge_page() {
    void* huge_page = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (huge_page == MAP_FAILED) {
        fprintf(stderr, "Failed to allocate huge page: %s\n", strerror(errno));

        // Fallback: tentar mmap normal
        huge_page = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (huge_page == MAP_FAILED) {
            fprintf(stderr, "Fallback allocation also failed: %s\n", strerror(errno));
            return NULL;
        } else {
            printf("Allocated normal page instead of huge page\n");
        }
    } else {
        printf("Successfully allocated huge page\n");
    }
    return huge_page;
}

int main() {

    iniciar_sistema_arquivos();
    allocate_huge_page();

    char command[20];
    char arg1[MAX_FILENAME_LENGTH], arg2[MAX_FILENAME_LENGTH];
    int arg3, arg4;

    while (1) {
        printf("> ");
        scanf("%s", command);
        
        if (strcmp(command, "criar") == 0) {
            scanf("%s %d", arg1, &arg3);
            criar(arg1, arg3);
        } else if (strcmp(command, "apagar") == 0) {
            scanf("%s", arg1);
            apagar(arg1);
        } else if (strcmp(command, "listar") == 0) {
            listar();
        } else if (strcmp(command, "ordenar") == 0) {
            scanf("%s", arg1);
            ordenar(arg1);
        } else if (strcmp(command, "ler") == 0) {
            scanf("%s %d %d", arg1, &arg3, &arg4);
            ler(arg1, arg3, arg4);
        } else if (strcmp(command, "concatenar") == 0) {
            scanf("%s %s", arg1, arg2);
            concatenar(arg1, arg2);
        } else if (strcmp(command, "sair") == 0) {
            break;
        } else {
            printf("Comando desconhecido\n");
        }
    }

    return 0;
}