Nomes: Francisco Borba e Matheus Magri
# sched\_profiler

## Descrição

O `sched_profiler` cria várias *threads* (tarefas) que escrevem em um buffer global compartilhado. Cada thread escreve seu próprio caractere (A, B, C, ...) no buffer. O acesso ao buffer é sincronizado com um semáforo para evitar conflitos entre as threads.

No final da execução, o programa mostra o conteúdo bruto do buffer (com todos os caracteres) e também uma versão pós-processada, que resume blocos iguais (por exemplo, "AAAAABBBBCC" vira "ABC"). Ele também mostra quantas vezes cada thread foi escalonada (executada) durante a execução.

---

## Como executar

```bash
./sched_profiler <tamanho_buffer> <num_threads> <policy>
```

### Parâmetros:

* `<tamanho_buffer>`: Número total de posições no buffer (quantas vezes as threads vão escrever no total).
* `<num_threads>`: Número de threads a serem criadas (máximo 26, uma para cada letra do alfabeto).
* `<policy>`: Política de escalonamento usada. Pode ser:

  * `SCHED_OTHER` (padrão do Linux)
  * `SCHED_FIFO` (fila de prioridade, tempo real)
  * `SCHED_RR` (round-robin, tempo real)
  * `SCHED_IDLE` (prioridade bem baixa)

---

## Exemplo de uso

```bash
./sched_profiler 1000 4 SCHED_RR
```

Este comando irá criar 4 threads, com buffer de 1000 posições, usando a política de escalonamento `SCHED_RR`.

---

## Saída esperada

### Buffer sem pós-processamento (exemplo):

```
AAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBCCCCCCCCCCCCCCCDDDDDDDDDDDDDD...
```

### Buffer pós-processado:

```
ABCD
```

### Contagem de escalonamentos por thread:

```
A = 248  
B = 250  
C = 251  
D = 251  
```

---

## Arquivos fornecidos na entrega

* `sched_profiler.c`: Código-fonte da aplicação.
* `README.md`: Este arquivo de explicação.
