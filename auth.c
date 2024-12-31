#include "auth.h"

int get_user_elo(const char *username) {
    FILE *file = fopen("elo_ratings.txt", "r");
    if (!file) {
        perror("Failed to open Elo ratings file");
        return 1000; // Default Elo if file is not accessible
    }

    char line[256];
    char stored_username[32];
    int stored_elo;

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %d", stored_username, &stored_elo) == 2) {
            if (strcmp(username, stored_username) == 0) {
                printf("tìm thấy tên ở đây này\n");
                fclose(file);
                return stored_elo; // Return Elo if username matches
            }
        }
    }

    fclose(file);
    return 1000; // Default Elo if user not found
}

int initialize_elo(const char *username) {
    FILE *file = fopen("elo_ratings.txt", "a+");
    if (!file) return 0;

    char file_username[50];
    while (fscanf(file, "%s", file_username) != EOF) {
        if (strcmp(username, file_username) == 0) {
            fclose(file);
            return 0;
        }
    }

    fprintf(file, "%s %d\n", username, 1000);
    fclose(file);
    return 1;
}

int validate_login(const char *username, const char *password) {
    FILE *file = fopen(ACCOUNT_FILE, "r");
    if (!file) return 0;

    char file_username[50], file_password[50];
    while (fscanf(file, "%s %s", file_username, file_password) != EOF) {
        if (strcmp(username, file_username) == 0 && strcmp(password, file_password) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

int register_user(const char *username, const char *password) {
    FILE *file = fopen(ACCOUNT_FILE, "a+");
    if (!file) return 0;

    char file_username[50];
    while (fscanf(file, "%s", file_username) != EOF) {
        if (strcmp(username, file_username) == 0) {
            fclose(file);
            return 0;
        }
    }

    fprintf(file, "%s %s\n", username, password);
    fclose(file);
    return 1;
}


void update_or_add_user_elo(const char *filename, const char *username, int newElo) {
    // Temporary file for writing
    const char *tempFile = "temp.txt";

    // Open the original file for reading
    FILE *filePointer = fopen(filename, "r");
    if (filePointer == NULL) {
        printf("Error opening file for reading!\n");
        return;
    }

    // Open the temporary file for writing
    FILE *tempPointer = fopen(tempFile, "w");
    if (tempPointer == NULL) {
        printf("Error opening temporary file for writing!\n");
        fclose(filePointer);
        return;
    }

    char line[MAX_LINE_LENGTH];
    char storedUsername[MAX_USERNAME_LENGTH];
    int storedElo;
    int found = 0;

    // Process each line
    while (fgets(line, sizeof(line), filePointer)) {
        if (sscanf(line, "%s %d", storedUsername, &storedElo) == 2) {
            if (strcmp(storedUsername, username) == 0) {
                // Update Elo if username matches
                fprintf(tempPointer, "%s %d\n", storedUsername, newElo);
                found = 1;
            } else {
                // Write unchanged line
                fprintf(tempPointer, "%s", line);
            }
        } else {
            // Write lines that don't match the expected format
            fprintf(tempPointer, "%s", line);
        }
    }

    // Append the new user if not found
    if (!found) {
        fprintf(tempPointer, "%s %d\n", username, newElo);
    }

    // Close files
    fclose(filePointer);
    fclose(tempPointer);

    // Replace the original file with the updated file
    if (remove(filename) != 0 || rename(tempFile, filename) != 0) {
        printf("Error updating the file!\n");
        return;
    }

    printf("File updated successfully!\n");
}
