#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "meustiposonlinegdb.h"
#include "tabela_de_frequencias.h"
#include "codigo.h"

#define MAX_FILENAME 256

// Função para criar a árvore de Huffman
Ptr_de_no_de_arvore_binaria criar_arvore_huffman(Tabela_de_frequencias* tab) {
    // Primeiro, juntamos todos os nós no início do vetor
    junte_nodos_no_inicio_do_vetor(tab);
    
    // Enquanto houver mais de um nó
    while (tab->quantidade_de_posicoes_preenchidas > 1) {
        // Pegamos os dois nós com menor frequência
        Ptr_de_no_de_arvore_binaria esq = tab->vetor[0];
        Ptr_de_no_de_arvore_binaria dir = tab->vetor[1];
        
        // Criamos um novo nó com a soma das frequências
        Elemento novo_elem;
        novo_elem.frequencia = esq->informacao.frequencia + dir->informacao.frequencia;
        
        // Criamos o novo nó
        Ptr_de_no_de_arvore_binaria novo_no;
        if (!novo_no_de_arvore_binaria(&novo_no, esq, novo_elem, dir)) {
            return NULL;
        }
        
        // Atualizamos a tabela
        tab->vetor[0] = novo_no;
        tab->vetor[1] = NULL;
        tab->quantidade_de_posicoes_preenchidas--;
        
        // Reorganizamos a tabela
        junte_nodos_no_inicio_do_vetor(tab);
    }
    
    return tab->vetor[0];
}

// Função para gerar os códigos de Huffman
void gerar_codigos_huffman(Ptr_de_no_de_arvore_binaria no, Codigo* codigo_atual, Codigo* codigos[256]) {
    if (no == NULL) return;
    
    // Se for uma folha (nó sem filhos)
    if (no->esquerda == NULL && no->direita == NULL) {
        // Clonamos o código atual para este byte
        codigos[no->informacao.byte] = (Codigo*)malloc(sizeof(Codigo));
        if (codigos[no->informacao.byte] == NULL) {
            printf("Erro ao alocar memória para código\n");
            return;
        }
        if (!clone(*codigo_atual, codigos[no->informacao.byte])) {
            printf("Erro ao clonar código\n");
            free(codigos[no->informacao.byte]);
            codigos[no->informacao.byte] = NULL;
            return;
        }
    }
    
    // Recursivamente para a esquerda (adiciona 0)
    if (no->esquerda != NULL) {
        adiciona_bit(codigo_atual, 0);
        gerar_codigos_huffman(no->esquerda, codigo_atual, codigos);
        joga_fora_bit(codigo_atual);
    }
    
    // Recursivamente para a direita (adiciona 1)
    if (no->direita != NULL) {
        adiciona_bit(codigo_atual, 1);
        gerar_codigos_huffman(no->direita, codigo_atual, codigos);
        joga_fora_bit(codigo_atual);
    }
}

void comprimir_arquivo(const char* nome_arquivo) {
    FILE* arquivo = fopen(nome_arquivo, "rb");
    if (!arquivo) {
        printf("Erro ao abrir o arquivo para leitura.\n");
        return;
    }

    // Criar tabela de frequências
    Tabela_de_frequencias tab;
    nova_tabela_de_frequencias(&tab);

    // Ler arquivo e contar frequências
    U8 byte;
    while (fread(&byte, 1, 1, arquivo) == 1) {
        if (!inclua_byte(byte, &tab)) {
            printf("Erro ao processar o arquivo.\n");
            fclose(arquivo);
            return;
        }
    }
    fclose(arquivo);

    // Criar árvore de Huffman
    Ptr_de_no_de_arvore_binaria raiz = criar_arvore_huffman(&tab);
    if (raiz == NULL) {
        printf("Erro ao criar árvore de Huffman.\n");
        return;
    }

    // Gerar códigos de Huffman
    Codigo codigo_atual;
    if (!novo_codigo(&codigo_atual)) {
        printf("Erro ao criar código.\n");
        return;
    }

    Codigo* codigos[256] = {NULL};
    gerar_codigos_huffman(raiz, &codigo_atual, codigos);

    // Criar arquivo comprimido
    char nome_comprimido[MAX_FILENAME];
    strcpy(nome_comprimido, nome_arquivo);
    strcat(nome_comprimido, ".comp");
    
    FILE* arquivo_comprimido = fopen(nome_comprimido, "wb");
    if (!arquivo_comprimido) {
        printf("Erro ao criar arquivo comprimido.\n");
        return;
    }

    // Escrever tabela de frequências
    fwrite(&tab.quantidade_de_posicoes_preenchidas, sizeof(U16), 1, arquivo_comprimido);
    for (U16 i = 0; i < 256; i++) {
        if (tab.vetor[i] != NULL) {
            fwrite(&i, sizeof(U8), 1, arquivo_comprimido);
            fwrite(&tab.vetor[i]->informacao.frequencia, sizeof(U64), 1, arquivo_comprimido);
        }
    }

    // Reabrir arquivo original para compressão
    arquivo = fopen(nome_arquivo, "rb");
    if (!arquivo) {
        printf("Erro ao reabrir arquivo original.\n");
        fclose(arquivo_comprimido);
        return;
    }

    // Comprimir dados
    U8 buffer = 0;
    int bits_no_buffer = 0;

    while (fread(&byte, 1, 1, arquivo) == 1) {
        if (codigos[byte] != NULL) {
            for (int i = 0; i < codigos[byte]->tamanho; i++) {
                // Pega o próximo bit do código
                U8 bit = (codigos[byte]->byte[i/8] >> (7 - (i % 8))) & 1;
                
                // Adiciona ao buffer
                buffer = (buffer << 1) | bit;
                bits_no_buffer++;
                
                // Se o buffer estiver cheio, escreve no arquivo
                if (bits_no_buffer == 8) {
                    fwrite(&buffer, 1, 1, arquivo_comprimido);
                    buffer = 0;
                    bits_no_buffer = 0;
                }
            }
        }
    }

    // Escreve os bits restantes no buffer
    if (bits_no_buffer > 0) {
        buffer <<= (8 - bits_no_buffer);
        fwrite(&buffer, 1, 1, arquivo_comprimido);
    }

    // Liberar memória
    for (int i = 0; i < 256; i++) {
        if (codigos[i] != NULL) {
            free_codigo(codigos[i]);
            free(codigos[i]);
        }
    }
    free_codigo(&codigo_atual);

    fclose(arquivo);
    fclose(arquivo_comprimido);
    printf("Arquivo comprimido com sucesso: %s\n", nome_comprimido);
}

void descomprimir_arquivo(const char* nome_arquivo) {
    FILE* arquivo = fopen(nome_arquivo, "rb");
    if (!arquivo) {
        printf("Erro ao abrir o arquivo comprimido.\n");
        return;
    }

    // Ler tabela de frequências
    Tabela_de_frequencias tab;
    nova_tabela_de_frequencias(&tab);

    U16 quantidade;
    if (fread(&quantidade, sizeof(U16), 1, arquivo) != 1) {
        printf("Erro ao ler quantidade de símbolos.\n");
        fclose(arquivo);
        return;
    }

    for (U16 i = 0; i < quantidade; i++) {
        U8 byte;
        U64 freq;
        if (fread(&byte, sizeof(U8), 1, arquivo) != 1 ||
            fread(&freq, sizeof(U64), 1, arquivo) != 1) {
            printf("Erro ao ler tabela de frequências.\n");
            fclose(arquivo);
            return;
        }
        inclua_byte(byte, &tab);
        tab.vetor[byte]->informacao.frequencia = freq;
    }

    // Criar árvore de Huffman
    Ptr_de_no_de_arvore_binaria raiz = criar_arvore_huffman(&tab);
    if (raiz == NULL) {
        printf("Erro ao criar árvore de Huffman.\n");
        fclose(arquivo);
        return;
    }

    // Criar arquivo descomprimido
    char nome_descomprimido[MAX_FILENAME];
    strcpy(nome_descomprimido, nome_arquivo);
    strcat(nome_descomprimido, ".decomp");
    
    FILE* arquivo_descomprimido = fopen(nome_descomprimido, "wb");
    if (!arquivo_descomprimido) {
        printf("Erro ao criar arquivo descomprimido.\n");
        fclose(arquivo);
        return;
    }

    // Descomprimir dados
    Ptr_de_no_de_arvore_binaria no_atual = raiz;
    U8 byte;
    U8 bit;
    int bits_lidos = 0;

    while (fread(&byte, 1, 1, arquivo) == 1) {
        for (int i = 7; i >= 0; i--) {
            bit = (byte >> i) & 1;
            
            if (bit == 0) {
                no_atual = no_atual->esquerda;
            } else {
                no_atual = no_atual->direita;
            }
            
            // Se chegamos em uma folha
            if (no_atual->esquerda == NULL && no_atual->direita == NULL) {
                fwrite(&no_atual->informacao.byte, 1, 1, arquivo_descomprimido);
                no_atual = raiz;
            }
        }
    }

    fclose(arquivo);
    fclose(arquivo_descomprimido);
    printf("Arquivo descomprimido com sucesso: %s\n", nome_descomprimido);
}

int main() {
    int opcao;
    char nome_arquivo[MAX_FILENAME];

    while (1) {
        printf("\n=== Menu de Compressão/Descompressão ===\n");
        printf("1. Comprimir arquivo\n");
        printf("2. Descomprimir arquivo\n");
        printf("3. Sair\n");
        printf("Escolha uma opção: ");
        scanf("%d", &opcao);
        getchar(); // Limpar o buffer do teclado

        switch (opcao) {
            case 1:
                printf("Digite o nome do arquivo para comprimir: ");
                fgets(nome_arquivo, MAX_FILENAME, stdin);
                nome_arquivo[strcspn(nome_arquivo, "\n")] = 0; // Remover \n
                comprimir_arquivo(nome_arquivo);
                break;

            case 2:
                printf("Digite o nome do arquivo para descomprimir: ");
                fgets(nome_arquivo, MAX_FILENAME, stdin);
                nome_arquivo[strcspn(nome_arquivo, "\n")] = 0; // Remover \n
                descomprimir_arquivo(nome_arquivo);
                break;

            case 3:
                printf("Saindo do programa...\n");
                return 0;

            default:
                printf("Opção inválida!\n");
        }
    }

    return 0;
} 