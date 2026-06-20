import re

with open('main/main.c', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

storage_code = """
    if (g_sd_card_initialized) {
        FATFS *fs;
        DWORD fre_clust, fre_sect, tot_sect;
        if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
            tot_sect = (fs->n_fatent - 2) * fs->csize;
            fre_sect = fre_clust * fs->csize;

            #if _MAX_SS != 512
            uint32_t sector_size = fs->ssize;
            #else
            uint32_t sector_size = 512;
            #endif

            // Convert to MB
            uint32_t total_mb = (tot_sect / 2048) * (sector_size / 512);
            uint32_t free_mb = (fre_sect / 2048) * (sector_size / 512);
            uint32_t used_mb = total_mb - free_mb;

            cJSON_AddNumberToObject(root, "sd_total_mb", total_mb);
            cJSON_AddNumberToObject(root, "sd_used_mb", used_mb);
        }
    }
"""

content = re.sub(
    r'(cJSON_AddBoolToObject\(root, "allow_public_uploads", allow_public_uploads\);)',
    r'\1\n' + storage_code,
    content
)

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)
