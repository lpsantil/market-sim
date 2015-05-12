#include "globals.h"

#include <curl/curl.h>

void cleanup(void)
{
    curl_global_cleanup();
}

struct data_buffer_t {
    char *data;
    uint back;
};

void all_upper(char *str)
{
    while(*str)
    {
        *str = toupper(*str);
        ++str;
    }
}

void all_lower(char *str)
{
    while(*str)
    {
        *str = tolower(*str);
        ++str;
    }
}

size_t download_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct data_buffer_t *buf = userdata;

    buf->data = realloc(buf->data, buf->back + size * nmemb);

    memcpy(buf->data + buf->back, ptr, size * nmemb);

    buf->back += size * nmemb;

    return size * nmemb;
}

bool get_stock_info(char *symbol, struct money_t *price, char **name_ret)
{
    CURL *curl = curl_easy_init();
    if(!curl)
    {
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://download.finance.yahoo.com/d/quotes.csv?s=%s&f=nl1&e=.csv", symbol);

    curl_easy_setopt(curl, CURLOPT_URL, url);

    struct data_buffer_t buf;
    memset(&buf, 0, sizeof(buf));

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_callback);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    /** now parse the data **/

    /* the stock name is in quotes, find it! */

    /* check for validity */
    if(buf.back == 0 || buf.data[0] != '"' || res != CURLE_OK)
    {
        printf("Failed to retrieve stock data.\n");
        if(res != CURLE_OK)
        {
            printf("Download library error (%d): '%s'\n", res, curl_easy_strerror(res));
        }
        return false;
    }

    uint name_len = 0;
    for(uint i = 1; i < buf.back; ++i)
    {
        if(buf.data[i] == '"')
            break;
        ++name_len;
    }

    const uint name_offs = 1;
    uint price_offs = name_len + 3;
    uint price_len = buf.back - price_offs;

    char *name = malloc(name_len + 1);
    memcpy(name, buf.data + name_offs, name_len);
    name[name_len] = '\0';

    *name_ret = name;

    /* get price */

    char *pricebuf = malloc(price_len + 1);
    memcpy(pricebuf, buf.data + price_offs, price_len);
    pricebuf[price_len] = '\0';

    free(buf.data);

    /* remove the decimal point */

    for(int i = 0; i < price_len; ++i)
    {
        if(pricebuf[i] == '.')
            memmove(pricebuf + i, pricebuf + i + 1, price_len - i);
    }

    price->cents = strtoull(pricebuf, NULL, 10);

    free(pricebuf);

    return true;
}

int compare_stocks(const void *a, const void *b)
{
    const struct stock_t *a1 = a, *b1 = b;
    return strcmp(a1->symbol,
                  b1->symbol);
}

static enum { OTHER = 0, LITTLE = 1, BIG = 2 } endianness;

static void detect_endianness(void)
{
    ulong test = 0x12345678;
    uchar *ptr = (uchar*)&test;
    if(*ptr == 0x12)
        endianness = BIG;
    else if(*ptr == 0x78)
        endianness = LITTLE;
    else
    {
        printf("FATAL: failed to detect system endianness!\n");
        exit(EXIT_FAILURE);
    }
}

uint64_t to_be64(uint64_t n)
{
    if(!endianness)
    {
        detect_endianness();
    }

    if(BIG)
    {
        n = (n & 0x00000000FFFFFFFF) << 32 | (n & 0xFFFFFFFF00000000) >> 32;
        n = (n & 0x0000FFFF0000FFFF) << 16 | (n & 0xFFFF0000FFFF0000) >> 16;
        n = (n & 0x00FF00FF00FF00FF) << 8  | (n & 0xFF00FF00FF00FF00) >> 8;
    }
    return n;
}

uint64_t to_sys64(uint64_t n)
{
    if(!endianness)
    {
        detect_endianness();
    }

    if(endianness == BIG)
        return n;
    else
        return to_be64(n);
}