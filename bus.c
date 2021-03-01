#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#define JUNCTION_SEM "/junction"
#define NORTH_SEM "/north"
#define EAST_SEM "/east"
#define SOUTH_SEM "/south"
#define WEST_SEM "/west"
#define FILE_SEM "/file"

#define BUFFER_LENGTH 50

void edit_matrix(int matrix_row, int matrix_column, int step);

int main(int argc, char *argv[])
{
  int matrix_row = atoi(argv[2]) + 1;
  int matrix_column;
  FILE *matrix, *temp;

  //open semaphores
  sem_t *direction, *right, *junction, *file_sem;

  if (0 == strcmp(argv[1], "north"))
  {
    direction = sem_open(NORTH_SEM, O_RDWR);
    right = sem_open(WEST_SEM, O_RDWR);
    matrix_column = 1;
  }
  else if (0 == strcmp(argv[1], "east"))
  {
    direction = sem_open(EAST_SEM, O_RDWR);
    right = sem_open(NORTH_SEM, O_RDWR);
    matrix_column = 4;
  }
  else if (0 == strcmp(argv[1], "south"))
  {
    direction = sem_open(SOUTH_SEM, O_RDWR);
    right = sem_open(EAST_SEM, O_RDWR);
    matrix_column = 3;
  }
  else if (0 == strcmp(argv[1], "west"))
  {
    direction = sem_open(WEST_SEM, O_RDWR);
    right = sem_open(SOUTH_SEM, O_RDWR);
    matrix_column = 2;
  }

  junction = sem_open(JUNCTION_SEM, O_RDWR);
  file_sem = sem_open(FILE_SEM, O_RDWR);

  //request semaphore for direction bus is coming from
  sem_wait(file_sem);
  edit_matrix(matrix_row, matrix_column, 0);
  sem_post(file_sem);

  printf("Bus <%d> requests for %s\n", matrix_row, argv[1]);

  //wait to get direction semaphore
  sem_wait(direction);

  //bus direction sempahore acquired
  sem_wait(file_sem);
  edit_matrix(matrix_row, matrix_column, 1);
  sem_post(file_sem);

  printf("Bus <%d> acquires %s\n", matrix_row, argv[1]);

  //request semaphore for direction to the right of bus
  sem_wait(file_sem);
  edit_matrix(matrix_row, matrix_column, 2);
  sem_post(file_sem);

  printf("Bus <%d> requests right of %s\n", matrix_row, argv[1]);

  //wait to get right semaphore
  sem_wait(right);

  //bus right sempahore acquired
  printf("Bus <%d> acquires right of %s\n", matrix_row, argv[1]);

  //request semaphore to enter the junction
  printf("Bus <%d> requests junction\n", matrix_row);
  sem_wait(junction);

  printf("Bus <%d> acquires junction; Passing junction\n", matrix_row);

  //cross the junction
  sleep(2);

  sem_wait(file_sem);
  edit_matrix(matrix_row, matrix_column, 3);
  sem_post(file_sem);

  //release semaphores now that bus has passed
  printf("Bus <%d> releases junction\n", matrix_row);
  sem_post(junction);

  printf("Bus <%d> releases %s\n", matrix_row, argv[1]);
  sem_post(direction);

  printf("Bus <%d> releases right of %s\n", matrix_row, argv[1]);
  sem_post(right);

  //close semaphores now that bus has passed
  sem_close(direction);
  sem_close(right);
  sem_close(junction);
  sem_close(file_sem);

  return 0;
}

void edit_matrix(int matrix_row, int matrix_column, int step)
{
  FILE *matrix = fopen("matrix.txt", "r");
  FILE *temp = fopen("temp.txt", "w");
  char str[BUFFER_LENGTH];
  char newln[BUFFER_LENGTH];
  int counter = 0;

  //determine new line to use in replacement
  switch (step)
  {
    case 0:
      if (1 == matrix_column)
        strcpy(newln, "1 0 0 0\n");
      if (2 == matrix_column)
        strcpy(newln, "0 1 0 0\n");
      if (3 == matrix_column)
        strcpy(newln, "0 0 1 0\n");
      if (4 == matrix_column)
        strcpy(newln, "0 0 0 1\n");
      break;
    case 1:
      if (1 == matrix_column)
        strcpy(newln, "2 0 0 0\n");
      if (2 == matrix_column)
        strcpy(newln, "0 2 0 0\n");
      if (3 == matrix_column)
        strcpy(newln, "0 0 2 0\n");
      if (4 == matrix_column)
        strcpy(newln, "0 0 0 2\n");
      break;
    case 2:
      if (1 == matrix_column)
        strcpy(newln, "2 1 0 0\n");
      if (2 == matrix_column)
        strcpy(newln, "0 2 1 0\n");
      if (3 == matrix_column)
        strcpy(newln, "0 0 2 1\n");
      if (4 == matrix_column)
        strcpy(newln, "1 0 0 2\n");
      break;
    case 3:
      strcpy(newln, "0 0 0 0\n");
      break;
  }
  //copy all contents to the temporary file except specific line
  while (!feof(matrix))
  {
    strcpy(str, "\0");
    fgets(str, BUFFER_LENGTH, matrix);
    if (!feof(matrix))
    {
      counter++;

      if (counter != matrix_row)
        fprintf(temp, "%s", str);
      else
        fprintf(temp, "%s", newln);
    }
  }
  //close files and make temporary file matrix.txt
  fclose(matrix);
  fclose(temp);
  remove("matrix.txt");
  rename("temp.txt", "matrix.txt");
}
