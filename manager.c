#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define JUNCTION_SEM "/junction"
#define NORTH_SEM "/north"
#define EAST_SEM "/east"
#define SOUTH_SEM "/south"
#define WEST_SEM "/west"
#define FILE_SEM "/file"

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define INITIAL_VALUE 1

#define BUFFER_LENGTH 50

#define CHILD_PROGRAM "./bus"

void unlink_semaphores();
int check_deadlock();

int main(int argc, char *argv[])
{
  //prepare probability of check for deadlock
  if (argc != 2)
  {
    printf("Probability between 0.2 and 0.7 required. Quitting.\n");
    return -1;
  }
  int check_probability = atof(argv[1]) * 10;
  if (check_probability < 2 || check_probability > 7)
  {
    printf("Probability between 0.2 and 0.7 required. Quitting.\n");
    return -1;
  }
  time_t t;
  srand((unsigned) time(&t));

  //read bus sequence
  FILE *sequence = fopen("sequence.txt", "r");
  char buses[BUFFER_LENGTH];
  int count = 0;
  while((buses[count] = getc(sequence)) != EOF)
  {
    printf("%c", buses[count]);
    count++;
  }
  fclose(sequence);

  //initialize direction semaphore matrix
  FILE *matrix = fopen("matrix.txt", "w");
  for (int i = 0; i < count - 1; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      fputs("0 ", matrix);
    }
    fputs("\n", matrix);
  }
  fclose(matrix);

  //initialize semaphores (unlink first if they already exist)
  unlink_semaphores();
  sem_t *junction = sem_open(JUNCTION_SEM, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);
  sem_t *file_sem = sem_open(FILE_SEM, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);
  sem_t *north = sem_open(NORTH_SEM, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);
  sem_t *east = sem_open(EAST_SEM, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);
  sem_t *south = sem_open(SOUTH_SEM, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);
  sem_t *west = sem_open(WEST_SEM, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);

  //close semaphores not used in the parent process
  sem_close(junction);
  sem_close(north);
  sem_close(east);
  sem_close(south);
  sem_close(west);

  //create children bus processes
  pid_t pids[count];
  size_t i = 0;
  while (i < (sizeof(pids) / sizeof(pids[0])) - 1)
  {
    //convert pid_t to int
    char number[5];
    sprintf(number, "%d", i);

    int random = (rand() % 10) + 1;

    //check matrix for deadlock
    if (random <= check_probability)
    {
      sem_wait(file_sem);
      check_deadlock();
      sem_post(file_sem);
    }
    //create next bus
    else
    {
      pids[i] = fork();

      if (pids[i] == 0)
      {
        //give child same pid as parent so all can be terminated together
        setpgid(0,0);

        printf("Bus <%d> %c bus started\n", i + 1, buses[i]);
        switch(buses[i])
        {
          case 'N':
            execl(CHILD_PROGRAM, CHILD_PROGRAM, "north", number, NULL);
          case 'E':
            execl(CHILD_PROGRAM, CHILD_PROGRAM, "east", number, NULL);
          case 'S':
            execl(CHILD_PROGRAM, CHILD_PROGRAM, "south", number, NULL);
          case 'W':
            execl(CHILD_PROGRAM, CHILD_PROGRAM, "west", number, NULL);
        }
      }
      i++;
    }
  }

  //create timer in order to break potential infinite loop below
  time_t current_time = time(NULL);
  time_t fire_time = time(NULL) + (count * 4); //give each bus 4 seconds

  //check for deadlock every second
  int deadlock = 0;
  while(!deadlock)
  {
    sleep(1);
    deadlock = check_deadlock();

    current_time = time(NULL);
    //if there is no deadlock by now, stop checking
    if (current_time > fire_time)
      break;
  }
  //no longer using file sempahore and semaphores
  sem_close(file_sem);
  unlink_semaphores();

  //terminate all processes
  if (deadlock)
    kill(0, SIGINT);

  //wait for all children to finish
  for (i = 0; i < sizeof(pids)/sizeof(pids[0]) - 1; i++)
    waitpid(pids[i], NULL, 0);

  return 0;
}

void unlink_semaphores()
{
  sem_unlink(JUNCTION_SEM);
  sem_unlink(FILE_SEM);
  sem_unlink(NORTH_SEM);
  sem_unlink(EAST_SEM);
  sem_unlink(SOUTH_SEM);
  sem_unlink(WEST_SEM);
}

int check_deadlock()
{
  FILE *matrix = fopen("matrix.txt", "r");
  char str[BUFFER_LENGTH];
  int bus_counter = 0;
  int line_counter = 1;
  int north_bus, east_bus, south_bus, west_bus;

  while (fgets(str, sizeof(str), matrix))
  {
    if (0 == strcmp(str, "2 1 0 0\n"))
    {
      north_bus = line_counter;
      bus_counter++;
    }
    else if (0 == strcmp(str, "0 2 1 0\n"))
    {
      west_bus = line_counter;
      bus_counter++;
    }
    else if (0 == strcmp(str, "0 0 2 1\n"))
    {
      south_bus = line_counter;
      bus_counter++;
    }
    else if (0 == strcmp(str, "1 0 0 2\n"))
    {
      east_bus = line_counter;
      bus_counter++;
    }
    line_counter++;
  }
  fclose(matrix);

  if (4 == bus_counter)
  {
    printf("SYSTEM DEADLOCKED\n");
    printf("Bus <%d> from north is waiting on Bus <%d>\n", north_bus, west_bus);
    printf("Bus <%d> from west is waiting on Bus <%d>\n", west_bus, south_bus);
    printf("Bus <%d> from south is waiting on Bus <%d>\n", south_bus, east_bus);
    printf("Bus <%d> from east is waiting on Bus <%d>\n", east_bus, north_bus);
    return 1;
  }
  else
    return 0;
}
