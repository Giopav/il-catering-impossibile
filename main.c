#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    char *nome;
    int id;
} piatto;

// variabili menu
static piatto *menu;
static int lunghezza_menu;

// variabili riga
static char *buffer_riga = NULL;

// variabili richieste e dipendenti
static int *richieste;
static int *inizio_richieste;
static size_t richieste_lette;
static size_t dipendenti_letti;

// variabili risoluzione
static signed char *assegnamenti;
static int *traccia_piatti;
static size_t lunghezza_traccia_piatti;
static int *scommesse;
static int *traccia_pre_scommessa;
static char *richiesta_girata;
static int scommesse_attive;

static int *occorrenze;
static int *inizio_occorrenze;
static int *conta_indecise;
static int *conta_vere;
static int *traccia_richieste;
static size_t limite_dipendenti;

// coda di propagazione
static int *coda_unitarie;
static size_t coda_testa, coda_piedi;
static char *in_coda;

// inizio funzioni di lettura e ottimizzazione del file

static int leggi_riga(void) {
    static size_t capienza_riga = 0;
    if (capienza_riga == 0) {
        capienza_riga = 256;
        buffer_riga = malloc(capienza_riga * sizeof(*buffer_riga));
        if (!buffer_riga) exit(2);
    }
    size_t lunghezza_riga = 0;

    while (1) {
        if (fgets(buffer_riga + lunghezza_riga, (int) (capienza_riga - lunghezza_riga), stdin) == NULL) {
            if (lunghezza_riga > 0) break;
            return -1;
        }

        lunghezza_riga += strlen(buffer_riga + lunghezza_riga);

        if (lunghezza_riga > 0 && buffer_riga[lunghezza_riga - 1] == '\n') break;
        size_t capienza_nuova = capienza_riga * 2;
        buffer_riga = realloc(buffer_riga, capienza_nuova * sizeof(*buffer_riga)); // NOLINT: exit() non permette perdite.
        if (!buffer_riga) exit(3);
        capienza_riga = capienza_nuova;
    }

    if (lunghezza_riga > 0 && buffer_riga[lunghezza_riga - 1] == '\n') lunghezza_riga--;
    if (lunghezza_riga > 0 && buffer_riga[lunghezza_riga - 1] == '\r') lunghezza_riga--;
    buffer_riga[lunghezza_riga] = '\0';

    return (int) lunghezza_riga;
}

static int conta_token(const char *cursore) {
    int numero_token = 0;
    while (*cursore) {
        while (*cursore && isspace((unsigned char)*cursore)) cursore++;
        if (*cursore) {
            numero_token++;
            while (*cursore && !isspace((unsigned char)*cursore)) cursore++;
        }
    }
    return numero_token;
}

static int confronta_piatti(const void *piatto_a, const void *piatto_b) {
    return strcmp(((const piatto*)piatto_a)->nome,((const piatto*)piatto_b)->nome);
}

static void genera_menu(int lunghezza_riga) {
    lunghezza_menu = conta_token(buffer_riga);
    menu = malloc(lunghezza_menu * sizeof(*menu));
    if (!menu) exit(4);

    char *riga_menu = malloc(lunghezza_riga + 1);
    if (!riga_menu) exit(5);
    memcpy(riga_menu, buffer_riga, lunghezza_riga + 1);

    int posizione_menu = 0;
    for (char *token = strtok(riga_menu, " \t\v\f\r"); token; token = strtok(NULL, " \t\v\f\r")) {
        menu[posizione_menu].nome = token;
        menu[posizione_menu].id = posizione_menu + 1;
        posizione_menu++;
    }
    qsort(menu, lunghezza_menu, sizeof(*menu), confronta_piatti);
}

static int fetch_id_piatto(const char *nome) {
    piatto cercato = { (char*)nome, 0 };
    piatto *trovato = bsearch(&cercato, menu, lunghezza_menu, sizeof(*menu), confronta_piatti);
    return trovato ? trovato->id : 0;                  /* 0 = non trovato */
}

static int fetch_richiesta(const char *token) {
    if (*token == '-') {
        int id = fetch_id_piatto(token + 1);
        return id ? -id : 0;
    }
    return fetch_id_piatto(token);
}

static void aggiungi_richiesta(int nuova_richiesta) {
    static size_t capienza_richieste = 0;
    if (richieste_lette == capienza_richieste) {
        size_t capienza_nuova = (capienza_richieste != 0) ? capienza_richieste * 2 : 256;
        richieste = realloc(richieste, capienza_nuova * sizeof(*richieste)); // NOLINT: exit() non permette perdite.
        if (!richieste) exit(6);

        capienza_richieste = capienza_nuova;
    }
    richieste[richieste_lette++] = nuova_richiesta;
}

static void assicura_capienza_dipendenti(void) {
    static size_t capienza_dipendenti = 0;
    if (dipendenti_letti + 1 >= capienza_dipendenti) {
        size_t capienza_nuova = (capienza_dipendenti != 0) ? capienza_dipendenti * 2 : 256;
        inizio_richieste = realloc(inizio_richieste, capienza_nuova * sizeof(*inizio_richieste)); // NOLINT: exit() non permette perdite.
        if (!inizio_richieste) exit(7);
        capienza_dipendenti = capienza_nuova;
    }
}

static void leggi_dipendenti(void) {
    int *visto_positivo = calloc(lunghezza_menu + 1, sizeof(*visto_positivo));
    if (!visto_positivo) exit(8);
    int *visto_negativo = calloc(lunghezza_menu + 1, sizeof(*visto_negativo));
    if (!visto_negativo) exit(9);

    while (leggi_riga() != -1) {
        assicura_capienza_dipendenti();
        inizio_richieste[dipendenti_letti] = (int) richieste_lette;
        int riga = (int) dipendenti_letti + 1;

        for (char *token = strtok(buffer_riga, " \t\v\f\r"); token != NULL; token = strtok(NULL, " \t\v\f\r")) {
            int richiesta = fetch_richiesta(token);
            if (richiesta == 0) continue;
            int id = richiesta > 0 ? richiesta : -richiesta;

            if (richiesta > 0) {
                if (visto_positivo[id] == riga) continue;
                if (visto_negativo[id] == riga) {
                    richieste_lette = inizio_richieste[dipendenti_letti];
                    aggiungi_richiesta(0);
                    break;
                }
                visto_positivo[id] = riga;
            } else {
                if (visto_negativo[id] == riga) continue;
                if (visto_positivo[id] == riga) {
                    richieste_lette = inizio_richieste[dipendenti_letti];
                    aggiungi_richiesta(0);
                    break;
                }
                visto_negativo[id] = riga;
            }
            aggiungi_richiesta(richiesta);
        }
        dipendenti_letti++;
    }

    assicura_capienza_dipendenti();
    inizio_richieste[dipendenti_letti] = (int) richieste_lette;

    if (richieste_lette > 0) {
        richieste = realloc(richieste, richieste_lette * sizeof(*richieste)); // NOLINT: exit() non permette perdite.
        if (!richieste) exit(10);
    }
    inizio_richieste = realloc(inizio_richieste, (dipendenti_letti + 1) * sizeof(*inizio_richieste)); // NOLINT: exit() non permette perdite.
    if (!inizio_richieste) exit(11);

    free(visto_positivo);
    free(visto_negativo);
}

// preparazione controllo menu soddisfacente

#define RICHIESTA_INDICIZZATA(r) (((r) > 0) ? (r) : (lunghezza_menu + (-(r))))

static void costruisci_occorrenze(void) {
    const int lunghezza_occorrenze = 2 * lunghezza_menu + 2;
    inizio_occorrenze = calloc(lunghezza_occorrenze + 1, sizeof(*inizio_occorrenze));
    if (!inizio_occorrenze) exit(13);

    for (size_t dipendente = 0; dipendente < dipendenti_letti; dipendente++)
        for (int posizione = inizio_richieste[dipendente]; posizione < inizio_richieste[dipendente+1]; posizione++)
            if (richieste[posizione] != 0) inizio_occorrenze[RICHIESTA_INDICIZZATA(richieste[posizione])+1]++;

    for (int occorrenza = 0; occorrenza < lunghezza_occorrenze; occorrenza++)
        inizio_occorrenze[occorrenza+1] += inizio_occorrenze[occorrenza];

    occorrenze = malloc((richieste_lette > 0 ? richieste_lette : 1) * sizeof(*occorrenze));
    if (!occorrenze) exit(14);
    int *cursore = malloc((lunghezza_occorrenze + 1) * sizeof(*cursore));
    if (!cursore) exit(15);
    memcpy(cursore, inizio_occorrenze, (lunghezza_occorrenze + 1) * sizeof(*cursore));
    for (size_t dipendente = 0; dipendente < dipendenti_letti; dipendente++)
        for (int richiesta = inizio_richieste[dipendente]; richiesta < inizio_richieste[dipendente+1]; richiesta++)
            if (richieste[richiesta] != 0) occorrenze[cursore[RICHIESTA_INDICIZZATA(richieste[richiesta])]++] = (int) dipendente;
    free(cursore);
}

static void inizializza_risolutore(void) {
    assegnamenti = calloc(lunghezza_menu + 1, sizeof(*assegnamenti));
    traccia_piatti = malloc(lunghezza_menu * sizeof(*traccia_piatti));
    traccia_pre_scommessa = malloc((lunghezza_menu + 1) * sizeof(*traccia_pre_scommessa));
    scommesse = malloc((lunghezza_menu + 1) * sizeof(*scommesse));
    richiesta_girata = malloc((lunghezza_menu + 1) * sizeof(*richiesta_girata));
    traccia_richieste = malloc((lunghezza_menu + 1) * sizeof(*traccia_richieste));
    conta_indecise = malloc((dipendenti_letti + 1) * sizeof(*conta_indecise));
    conta_vere = malloc((dipendenti_letti + 1) * sizeof(*conta_vere));
    coda_unitarie = malloc((dipendenti_letti + 1) * sizeof(*coda_unitarie));
    in_coda = calloc(dipendenti_letti + 1, sizeof(*in_coda));
    if (!assegnamenti || !traccia_piatti || !traccia_pre_scommessa || !scommesse || !richiesta_girata
        || !traccia_richieste || !conta_indecise || !conta_vere || !coda_unitarie || !in_coda) exit(12);
    costruisci_occorrenze();
}

static void aggiorna_contatori(int richiesta, int verso) {
    const int richiesta_soddisfatta = RICHIESTA_INDICIZZATA(richiesta);
    for (int posizione = inizio_occorrenze[richiesta_soddisfatta];
        posizione < inizio_occorrenze[richiesta_soddisfatta+1]; posizione++) {
        const int dipendente = occorrenze[posizione];
        if ((size_t) dipendente >= limite_dipendenti) continue;
        conta_vere[dipendente] += verso;
        conta_indecise[dipendente] -= verso;
    }

    const int richiesta_insoddisfatta = RICHIESTA_INDICIZZATA(-richiesta);
    for (int posizione = inizio_occorrenze[richiesta_insoddisfatta];
        posizione < inizio_occorrenze[richiesta_insoddisfatta+1]; posizione++) {
        const int dipendente = occorrenze[posizione];
        if ((size_t) dipendente >= limite_dipendenti) continue;
        conta_indecise[dipendente] -= verso;
        if (verso > 0 && conta_vere[dipendente] == 0 && conta_indecise[dipendente] <= 1 && !in_coda[dipendente]) {
            in_coda[dipendente] = 1;
            coda_unitarie[coda_piedi++] = dipendente;
        }
    }
}

static void assegna(int richiesta) {
    const int id_piatto = richiesta > 0 ? richiesta : -richiesta;
    assegnamenti[id_piatto] = (richiesta > 0) ? 1 : -1;
    traccia_piatti[lunghezza_traccia_piatti] = id_piatto;
    traccia_richieste[lunghezza_traccia_piatti] = richiesta;
    lunghezza_traccia_piatti++;
    aggiorna_contatori(richiesta, 1);
}

static void disfa_fino_a(int lunghezza_bersaglio) {
    while ((int) lunghezza_traccia_piatti > lunghezza_bersaglio) {
        lunghezza_traccia_piatti--;
        aggiorna_contatori(traccia_richieste[lunghezza_traccia_piatti], -1);
        assegnamenti[traccia_piatti[lunghezza_traccia_piatti]] = 0;
    }
}

static int fetch_richiesta_obbligata(int dipendente) {
    for (int posizione = inizio_richieste[dipendente]; posizione < inizio_richieste[dipendente+1]; posizione++) {
        const int richiesta_indecisa = richieste[posizione];
        if (richiesta_indecisa != 0 && assegnamenti[richiesta_indecisa > 0 ? richiesta_indecisa : -richiesta_indecisa] == 0)
            return richiesta_indecisa;
    }
    return 0;
}

static int fetch_richiesta_massima(int dipendente) {
    int richiesta_massima = 0, occorrenze_massime = -1;
    for (int posizione = inizio_richieste[dipendente]; posizione < inizio_richieste[dipendente+1]; posizione++) {
        const int richiesta = richieste[posizione];
        if (richiesta == 0) continue;
        if (assegnamenti[richiesta > 0 ? richiesta : -richiesta] != 0) continue;
        const int indice_richiesta = RICHIESTA_INDICIZZATA(richiesta);
        const int numero_occorrenze = inizio_occorrenze[indice_richiesta+1] - inizio_occorrenze[indice_richiesta];
        if (numero_occorrenze > occorrenze_massime) {
            occorrenze_massime = numero_occorrenze;
            richiesta_massima = richiesta;
        }
    }
    return richiesta_massima;
}

// inizio controllo menu soddisfacentex

static void svuota_coda(void) {
    while (coda_testa < coda_piedi) in_coda[coda_unitarie[coda_testa++]] = 0;
    coda_testa = coda_piedi = 0;
}

static int propaga(void) {
    while (coda_testa < coda_piedi) {
        const int dipendente = coda_unitarie[coda_testa++];
        in_coda[dipendente] = 0;
        if ((size_t) dipendente >= limite_dipendenti) continue;
        if (conta_vere[dipendente] != 0) continue;
        if (conta_indecise[dipendente] == 0) return 0;
        if (conta_indecise[dipendente] == 1) assegna(fetch_richiesta_obbligata(dipendente));
    }
    return 1;
}

static int scommetti(void) {
    int dipendente_migliore = -1, indecise_dipendente_migliore = 0;
    for (size_t dipendente = 0; dipendente < limite_dipendenti; dipendente++) {
        if (conta_vere[dipendente] != 0) continue;
        const int indecise = conta_indecise[dipendente];
        if (indecise == 0) continue;
        if (dipendente_migliore < 0 || indecise < indecise_dipendente_migliore) {
            dipendente_migliore = (int) dipendente;
            indecise_dipendente_migliore = indecise;
            if (indecise == 2) break;
        }
    }
    return dipendente_migliore < 0 ? 0 : fetch_richiesta_massima(dipendente_migliore);
}

static int risolvi_fino_a(size_t dipendenti_considerati) {
    limite_dipendenti = dipendenti_considerati;
    memset(assegnamenti, 0, (lunghezza_menu + 1) * sizeof(*assegnamenti));
    lunghezza_traccia_piatti = 0;
    scommesse_attive = 0;

    for (size_t dipendente = 0; dipendente < limite_dipendenti; dipendente++) {
        const int primo = inizio_richieste[dipendente], fine = inizio_richieste[dipendente+1];
        if (fine - primo == 1 && richieste[primo] == 0) {
            conta_vere[dipendente] = 1;
            conta_indecise[dipendente] = 0;
        } else {
            conta_vere[dipendente] = 0;
            conta_indecise[dipendente] = fine - primo;
        }
    }

    svuota_coda();
    for (size_t dipendente = 0; dipendente < limite_dipendenti; dipendente++) {
        if (conta_vere[dipendente] == 0 && conta_indecise[dipendente] <= 1 && !in_coda[dipendente]) {
            in_coda[dipendente] = 1;
            coda_unitarie[coda_piedi++] = (int) dipendente;
        }
    }

    while (1) {
        if (propaga()) {
            int richiesta_scommessa = scommetti();
            if (richiesta_scommessa == 0) return 1;
            traccia_pre_scommessa[scommesse_attive] = (int) lunghezza_traccia_piatti;
            scommesse[scommesse_attive] = richiesta_scommessa;
            richiesta_girata[scommesse_attive] = 0;
            scommesse_attive++;
            assegna(richiesta_scommessa);
        } else {
            svuota_coda();
            while (scommesse_attive > 0 && richiesta_girata[scommesse_attive - 1]) {
                scommesse_attive--;
                disfa_fino_a(traccia_pre_scommessa[scommesse_attive]);
            }
            if (scommesse_attive == 0) return 0;
            scommesse_attive--;
            disfa_fino_a(traccia_pre_scommessa[scommesse_attive]);
            svuota_coda();
            richiesta_girata[scommesse_attive] = 1;
            scommesse[scommesse_attive] = -scommesse[scommesse_attive];
            scommesse_attive++;
            assegna(scommesse[scommesse_attive - 1]);
        }

    }
}

static void risolvi(void) {
    if (risolvi_fino_a(dipendenti_letti)) {
        printf("OK\n");
        return;
    }

    size_t dipendente_basso = 0, dipendente_alto = dipendenti_letti;
    size_t salto = 1;
    while (1) {
        if (salto >= dipendente_alto) {
            dipendente_basso = 0;
            break;
        }
        size_t candidato = dipendente_alto - salto;
        if (risolvi_fino_a(candidato)) {
            dipendente_basso = candidato;
            break;
        }
        dipendente_alto = candidato;
        salto *= 2;
    }
    while (dipendente_alto - dipendente_basso > 1) {
        size_t mezzo = dipendente_basso + (dipendente_alto - dipendente_basso) / 2;
        if (risolvi_fino_a(mezzo)) dipendente_basso = mezzo;
        else dipendente_alto = mezzo;
    }
    printf("KO\n");
    for (size_t i = 1; i <= dipendenti_letti - dipendente_basso; i++) printf("-%zu\n", i);
    printf("OK\n");
}

int main(void) {
    int lunghezza_riga_menu = leggi_riga();
    if (lunghezza_riga_menu <= 0) return -1;
    genera_menu(lunghezza_riga_menu);
    leggi_dipendenti();
    inizializza_risolutore();
    risolvi();
    return 0;
}