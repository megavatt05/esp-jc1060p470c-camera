"""
Диагностический скрипт: эмуляция поиска 'r' в каталоге фильмов archive.org
и проверка совместимости найденных фильмов с декодером tinyH264 / esp_h264 на ESP32-P4.
"""
import urllib.request
import json
import struct

def check_search_r():
    search_url = 'http://archive.org/advancedsearch.php?q=mediatype%3A%28movies%29%20AND%20%28title%3A%28r%29%20OR%20r%29&fl%5B%5D=identifier&fl%5B%5D=title&sort%5B%5D=-downloads&rows=10&page=1&output=json'
    req = urllib.request.Request(search_url, headers={'User-Agent': 'CamBrowser/1.0'})
    
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            docs = data.get('response', {}).get('docs', [])
    except Exception as e:
        print(f"Ошибка поиска: {e}")
        return

    print(f"=== НАЙДЕНО ФИЛЬМОВ ПО ЗАПРОСУ 'r': {len(docs)} ===")
    
    compatible_count = 0
    rejected_count = 0
    
    for idx, doc in enumerate(docs, 1):
        ident = doc['identifier']
        title = doc.get('title', ident)
        meta_url = f"http://archive.org/metadata/{ident}"
        
        try:
            with urllib.request.urlopen(urllib.request.Request(meta_url, headers={'User-Agent': 'CamBrowser/1.0'}), timeout=10) as mresp:
                mdata = json.loads(mresp.read().decode('utf-8'))
                files = mdata.get('files', [])
                
                # Точная логика pick_cb из main/browser/films.c:
                best = None
                for f in files:
                    name = f.get('name', '')
                    if not name.lower().endswith('.mp4'):
                        continue
                    size = int(f.get('size', 0))
                    if size > 900 * 1000 * 1000:
                        continue
                    if '512kb' in name.lower():
                        best = f
                        break
                    if best is None or size < int(best.get('size', 0)):
                        best = f
                
                if not best:
                    print(f"[{idx}] '{title}' -> ОТКЛОНЕНО: Нет MP4 файла <900MB (films_resolve: FILMS_ERR_NO_MP4)")
                    rejected_count += 1
                    continue
                
                file_name = best.get('name')
                fmt = best.get('format', 'MPEG4')
                server = mdata.get('server')
                dir_path = mdata.get('dir')
                
                # Проверяем, кодек H.264 или MPEG-4 Part 2
                if fmt == '512Kb MPEG4' or fmt == 'MPEG4':
                    print(f"[{idx}] '{title}' -> ОТКЛОНЕНО: Формат '{fmt}' (MPEG-4 Part 2, FourCC mp4v). Декодер ESP32-P4 поддерживает ТОЛЬКО H.264 (avc1)!")
                    rejected_count += 1
                    continue
                
                # Если h.264, проверяем Range request к файлу
                if server and dir_path:
                    stream_url = f"http://{server}{dir_path}/{urllib.parse.quote(file_name)}"
                else:
                    stream_url = f"http://archive.org/download/{ident}/{urllib.parse.quote(file_name)}"
                
                # Делаем частичный запрос Range для проверки moov / avcC
                range_req = urllib.request.Request(stream_url, headers={'Range': 'bytes=0-65535', 'User-Agent': 'CamBrowser/1.0'})
                try:
                    with urllib.request.urlopen(range_req, timeout=10) as sresp:
                        header_bytes = sresp.read(65536)
                        # Ищем avcC или profile
                        avcc_idx = header_bytes.find(b'avcC')
                        if avcc_idx != -1 and avcc_idx + 8 <= len(header_bytes):
                            profile = header_bytes[avcc_idx + 5]
                            compat = header_bytes[avcc_idx + 6]
                            has_ctts = (b'ctts' in header_bytes)
                            
                            is_baseline = (profile == 66) or (profile == 77 and (compat & 0x40) and not has_ctts)
                            if is_baseline:
                                print(f"[{idx}] '{title}' -> СОВМЕСТИМО! (H.264 Baseline)")
                                compatible_count += 1
                            else:
                                reason = f"Profile={profile}"
                                if profile == 100:
                                    reason = "H.264 High Profile (CABAC)"
                                elif has_ctts:
                                    reason += " с B-кадрами (ctts)"
                                print(f"[{idx}] '{title}' -> ОТКЛОНЕНО: {reason} (films_probe_url: FILMS_ERR_HIGH_PROFILE)")
                                rejected_count += 1
                        else:
                            # moov может быть в конце файла или дальше 64KB
                            print(f"[{idx}] '{title}' -> ОТКЛОНЕНО: moov atom не в первых 64КБ или профиль High (FILMS_ERR_BIG_MOOV / High Profile)")
                            rejected_count += 1
                except Exception as probe_err:
                    print(f"[{idx}] '{title}' -> ОШИБКА ДОСТУПА К ФАЙЛУ: {probe_err}")
                    rejected_count += 1
                    
        except Exception as e:
            print(f"[{idx}] '{title}' -> Ошибка метаданных: {e}")
            rejected_count += 1

    print("=" * 60)
    print(f"ИТОГ: Всего проверено: {len(docs)}, Совместимо: {compatible_count}, Не показывает: {rejected_count}")
    print("=" * 60)

if __name__ == '__main__':
    check_search_r()
