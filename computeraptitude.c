#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LEVELS 3
#define CAPACITY   4
#define DATA_FILE  "parking_data.txt"

/* ---------- Data structures ---------- */

typedef struct {
    char   numberPlate[15];
    int    slotNumber;
    int    level;
    time_t entryTime;
} Vehicle;

typedef struct {
    Vehicle *slots[CAPACITY];   /* stack of Vehicle pointers */
    int top;                    /* -1 when empty */
    int capacity;
    int levelNumber;
} ParkingLevel;

/* ---------- Helper functions ---------- */

void initLevel(ParkingLevel *l, int levelNumber, int capacity) {
    l->top = -1;
    l->capacity = capacity;
    l->levelNumber = levelNumber;
}

void fmtTime(time_t t, char *buf, size_t size) {
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%d-%m-%Y %H:%M:%S", tm_info);
}

/* ---------- Stack operations ---------- */

int isFull(ParkingLevel *l)  { return l->top == l->capacity - 1; }

int isEmpty(ParkingLevel *l) { return l->top == -1; }

int pushVehicle(ParkingLevel *l, Vehicle *v) {
    if (isFull(l)) return -1;
    l->slots[++l->top] = v;
    v->slotNumber = l->top + 1;
    v->level = l->levelNumber;
    return v->slotNumber;
}

Vehicle *popVehicle(ParkingLevel *l) {
    if (isEmpty(l)) return NULL;
    return l->slots[l->top--];
}

/* ---------- Parking and LIFO retrieval ---------- */

int parkVehicle(ParkingLevel *l, const char *plate, time_t when) {
    Vehicle *v = (Vehicle *)malloc(sizeof(Vehicle));
    if (!v) return -1;
    strncpy(v->numberPlate, plate, sizeof(v->numberPlate) - 1);
    v->numberPlate[sizeof(v->numberPlate) - 1] = '\0';
    v->entryTime = when;
    if (pushVehicle(l, v) == -1) { free(v); return -1; }
    return v->slotNumber;
}

/* Remove a vehicle anywhere in the lane: pop blockers into a helper
   stack, remove target, then push blockers back in original order. */
Vehicle *removeVehicle(ParkingLevel *l, const char *plate, int *moved) {
    Vehicle *helper[CAPACITY];
    int h = -1;
    Vehicle *target = NULL;
    *moved = 0;
    while (!isEmpty(l)) {
        Vehicle *v = popVehicle(l);
        if (strcmp(v->numberPlate, plate) == 0) { target = v; break; }
        helper[++h] = v;
        (*moved)++;
    }
    while (h >= 0) pushVehicle(l, helper[h--]);
    if (!target) *moved = 0;
    return target;
}

/* ---------- Multi-level allocation ---------- */

int findVehicleLevel(ParkingLevel lv[], int n, const char *plate) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j <= lv[i].top; j++)
            if (strcmp(lv[i].slots[j]->numberPlate, plate) == 0) return i;
    return -1;
}

int allocateVehicle(ParkingLevel lv[], int n, const char *plate, time_t when) {
    if (findVehicleLevel(lv, n, plate) != -1) {
        printf("Vehicle already parked.\n");
        return -1;
    }
    for (int i = 0; i < n; i++)
        if (!isFull(&lv[i])) {
            int s = parkVehicle(&lv[i], plate, when);
            printf("Vehicle %s parked at Level %d, Slot %d\n",
                   plate, lv[i].levelNumber, s);
            return s;
        }
    printf("Parking Full\n");
    return -1;
}

/* ---------- Display ---------- */

void displayAllLevels(ParkingLevel lv[], int n) {
    printf("\n========== PARKING STATUS ==========\n");
    for (int i = 0; i < n; i++) {
        printf("Level %d (%d/%d filled):\n", lv[i].levelNumber, lv[i].top + 1, lv[i].capacity);
        if (isEmpty(&lv[i])) {
            printf("   [empty]\n");
            continue;
        }
        for (int j = 0; j <= lv[i].top; j++) {
            char t[32];
            fmtTime(lv[i].slots[j]->entryTime, t, sizeof(t));
            printf("   Slot %d : %-14s (in: %s)%s\n", j + 1,
                   lv[i].slots[j]->numberPlate, t,
                   j == lv[i].top ? "  <-- nearest exit" : "");
        }
    }
    printf("====================================\n");
}

/* ---------- Billing and exit receipt ---------- */

double calculateDuration(time_t in, time_t out) {
    double secs = difftime(out, in);
    double hrs = secs / 3600.0;
    int whole = (int)hrs;
    if (hrs > whole || whole == 0) whole++;      /* round up, minimum 1 hour */
    return (double)whole;
}

double calculateFee(double hrs) { return hrs <= 1 ? 30.0 : 30.0 + (hrs - 1) * 15.0; }

void processExit(ParkingLevel lv[], int n, const char *plate, time_t now) {
    int i = findVehicleLevel(lv, n, plate);
    if (i < 0) { printf("Vehicle %s not found.\n", plate); return; }
    int moved;
    Vehicle *v = removeVehicle(&lv[i], plate, &moved);
    char a[32], b[32];
    fmtTime(v->entryTime, a, 32);
    fmtTime(now, b, 32);
    double hrs = calculateDuration(v->entryTime, now);
    printf("\n------------ PARKING RECEIPT ------------\n");
    printf("Vehicle      : %s\nLevel / Slot : %d / %d\n", v->numberPlate, v->level, v->slotNumber);
    printf("Entry Time   : %s\nExit Time    : %s\n", a, b);
    printf("Vehicles moved to free the lane: %d\n", moved);
    printf("Duration     : %.0f hour(s)\nTotal Fee    : Rs. %.2f\n", hrs, calculateFee(hrs));
    printf("-----------------------------------------\n");
    free(v);
}

/* ---------- Persistence (file handling) ---------- */

void saveState(ParkingLevel lv[], int n, const char *fn) {
    FILE *f = fopen(fn, "w");
    if (!f) { printf("Could not save.\n"); return; }
    for (int i = 0; i < n; i++)
        for (int j = 0; j <= lv[i].top; j++)      /* bottom -> top keeps order */
            fprintf(f, "%d %s %ld\n", lv[i].levelNumber, lv[i].slots[j]->numberPlate,
                    (long)lv[i].slots[j]->entryTime);
    fclose(f);
}

void loadState(ParkingLevel lv[], int n, const char *fn) {
    FILE *f = fopen(fn, "r");
    if (!f) return;                               /* no file: start empty */
    int lvl; char plate[15]; long t;
    while (fscanf(f, "%d %14s %ld", &lvl, plate, &t) == 3)
        if (lvl >= 1 && lvl <= n) parkVehicle(&lv[lvl - 1], plate, (time_t)t);
    fclose(f);
}

void freeAll(ParkingLevel lv[], int n) {          /* release memory on exit */
    for (int i = 0; i < n; i++)
        while (!isEmpty(&lv[i])) free(popVehicle(&lv[i]));
}

/* ---------- Menu and main program ---------- */

void printMenu(void) {
    printf("\n===== MULTI-LEVEL PARKING SYSTEM =====\n");
    printf("1. Park a Vehicle\n2. Remove a Vehicle (Exit + Billing)\n");
    printf("3. Display All Levels\n4. Search Vehicle by Number Plate\n");
    printf("5. Save & Exit\nEnter choice: ");
}

int main(void) {
    ParkingLevel lv[MAX_LEVELS];
    char plate[15];
    int ch;

    for (int i = 0; i < MAX_LEVELS; i++) initLevel(&lv[i], i + 1, CAPACITY);
    loadState(lv, MAX_LEVELS, DATA_FILE);

    do {
        printMenu();
        if (scanf("%d", &ch) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);   /* clear bad input */
            if (c == EOF) break;                           /* no input available: stop */
            ch = 0;
            continue;
        }
        switch (ch) {
            case 1:
                printf("Enter number plate: "); scanf("%14s", plate);
                allocateVehicle(lv, MAX_LEVELS, plate, time(NULL));
                break;
            case 2:
                printf("Enter number plate: "); scanf("%14s", plate);
                processExit(lv, MAX_LEVELS, plate, time(NULL));
                break;
            case 3:
                displayAllLevels(lv, MAX_LEVELS);
                break;
            case 4:
                printf("Enter number plate: "); scanf("%14s", plate);
                {
                    int i = findVehicleLevel(lv, MAX_LEVELS, plate);
                    if (i < 0) printf("Not found.\n");
                    else printf("%s is on Level %d\n", plate, i + 1);
                }
                break;
            case 5:
                saveState(lv, MAX_LEVELS, DATA_FILE);
                printf("State saved. Goodbye!\n");
                break;
            default:
                printf("Invalid choice.\n");
        }
    } while (ch != 5);

    freeAll(lv, MAX_LEVELS);
    return 0;
}