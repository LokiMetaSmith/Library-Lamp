#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_QUEUE_SIZE 50
#define MAX_FILENAME_LEN 128

typedef struct {
    char filename[MAX_FILENAME_LEN];
} audio_track_t;

static audio_track_t audio_queue[MAX_QUEUE_SIZE];
static int queue_count = 0;
static char current_track[MAX_FILENAME_LEN] = "";
static bool is_playing = false;
static float track_position_sec = 0.0;

bool audio_validate_filename(const char* filename) {
    if (!filename || strlen(filename) == 0) return false;

    // Validate filename against path traversal and sensitive files
    if (strstr(filename, "..") || strchr(filename, '/') || strchr(filename, '\\') ||
        strcasestr(filename, "adminkey.json") || strcasestr(filename, ".key")) {
        return false;
    }

    // Validate supported audio extensions in O(1) time
    const char *ext = strrchr(filename, '.');
    if (!ext || (strcasecmp(ext, ".mp3") != 0 &&
                 strcasecmp(ext, ".m4a") != 0 &&
                 strcasecmp(ext, ".wav") != 0 &&
                 strcasecmp(ext, ".ogg") != 0)) {
        return false;
    }

    return true;
}

bool audio_add_to_queue(const char* filename) {
    if (!audio_validate_filename(filename)) return false;
    if (queue_count >= MAX_QUEUE_SIZE) return false;

    strncpy(audio_queue[queue_count].filename, filename, MAX_FILENAME_LEN - 1);
    audio_queue[queue_count].filename[MAX_FILENAME_LEN - 1] = '\0';
    queue_count++;

    if (strlen(current_track) == 0) {
        strncpy(current_track, audio_queue[0].filename, MAX_FILENAME_LEN - 1);
        current_track[MAX_FILENAME_LEN - 1] = '\0';

        if (queue_count > 1) {
            memmove(&audio_queue[0], &audio_queue[1], (queue_count - 1) * sizeof(audio_track_t));
        }
        queue_count--;

        is_playing = true;
        track_position_sec = 0.0;
    }

    return true;
}

bool audio_delete_from_queue(int index) {
    if (index >= 0 && index < queue_count) {
        if (index < queue_count - 1) {
            memmove(&audio_queue[index], &audio_queue[index + 1], (queue_count - 1 - index) * sizeof(audio_track_t));
        }
        queue_count--;
        return true;
    }
    return false;
}

void audio_action_next(void) {
    if (queue_count > 0) {
        strncpy(current_track, audio_queue[0].filename, MAX_FILENAME_LEN - 1);
        current_track[MAX_FILENAME_LEN - 1] = '\0';

        if (queue_count > 1) {
            memmove(&audio_queue[0], &audio_queue[1], (queue_count - 1) * sizeof(audio_track_t));
        }
        queue_count--;

        is_playing = true;
        track_position_sec = 0.0;
    } else {
        current_track[0] = '\0';
        is_playing = false;
        track_position_sec = 0.0;
    }
}

void test_audio_validation() {
    assert(audio_validate_filename("song.mp3") == true);
    assert(audio_validate_filename("audio.m4a") == true);
    assert(audio_validate_filename("sound.wav") == true);
    assert(audio_validate_filename("podcast.ogg") == true);

    // Invalid extension
    assert(audio_validate_filename("book.epub") == false);
    assert(audio_validate_filename("malicious.sh") == false);
    assert(audio_validate_filename("no_ext") == false);

    // Path traversal
    assert(audio_validate_filename("../song.mp3") == false);
    assert(audio_validate_filename("dir/song.mp3") == false);
    assert(audio_validate_filename("dir\\song.mp3") == false);

    // Sensitive files
    assert(audio_validate_filename("adminkey.json.mp3") == false);
    assert(audio_validate_filename("test.key.mp3") == false);

    printf("test_audio_validation passed.\n");
}

void test_audio_queue_transitions() {
    queue_count = 0;
    current_track[0] = '\0';
    is_playing = false;

    // First track automatically starts playing
    assert(audio_add_to_queue("track1.mp3") == true);
    assert(strcmp(current_track, "track1.mp3") == 0);
    assert(is_playing == true);
    assert(queue_count == 0);

    // Second and third tracks stay in queue
    assert(audio_add_to_queue("track2.mp3") == true);
    assert(audio_add_to_queue("track3.mp3") == true);
    assert(queue_count == 2);
    assert(strcmp(audio_queue[0].filename, "track2.mp3") == 0);
    assert(strcmp(audio_queue[1].filename, "track3.mp3") == 0);

    // Delete middle track
    assert(audio_delete_from_queue(0) == true);
    assert(queue_count == 1);
    assert(strcmp(audio_queue[0].filename, "track3.mp3") == 0);

    // Next track advances
    audio_action_next();
    assert(strcmp(current_track, "track3.mp3") == 0);
    assert(queue_count == 0);
    assert(is_playing == true);

    // Next when queue is empty stops playing
    audio_action_next();
    assert(current_track[0] == '\0');
    assert(is_playing == false);

    printf("test_audio_queue_transitions passed.\n");
}

int main() {
    test_audio_validation();
    test_audio_queue_transitions();
    printf("All audio unit tests passed!\n");
    return 0;
}
