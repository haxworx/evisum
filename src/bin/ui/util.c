
// Process Window includes this junk.

static char *
_man2entry(const char *text)
{
   const char *p;
   char *str;
   void *tmp;
   int i = 0, len = strlen(text) + 1;

   str = malloc(len);
   p = text;

   while (*p)
     {
        if (*p == '<')
          {
             tmp = realloc(str, (len += 4));
             str = tmp;
             memcpy(&str[i], "&lt;", 4);
             i += 4;
          }
        else if (*p == '>')
          {
             tmp = realloc(str, (len += 4));
             str = tmp;
             memcpy(&str[i], "&gt;", 4);
             i += 4;
          }
        else if (*p == '&')
          {
             tmp = realloc(str, (len += 5));
             str = tmp;
             memcpy(&str[i], "&amp;", 5);
             i += 5;
          }
        else if (*p == '\t')
          {
             tmp = realloc(str, (len += 8));
             str = tmp;
             memcpy(&str[i], "        ", 8);
             i += 8;
          }
        else
          {
             str[i++] = *p;
          }
        p++;
     }
   str[i] = '\0';

   return str;
}

static const char *
_evisum_docs(void)
{
   const char *txt =
         "<b>Congratulations you found the documentation!</b><br><br>"
         "Control + K - Toggle kernel threads. <br>"
         "Control + E - Show Self. <br>";

   return txt;
}
