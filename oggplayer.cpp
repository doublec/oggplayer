#include <cstring>
#include <iostream>
#include <oggplay/oggplay.h>
#include <boost/shared_ptr.hpp>

using namespace std;
using namespace boost;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cout << "Usage: oggplayer <filename>" << endl;
    return -1;
  }

  OggPlayReader* reader = 0;
  if (strncmp(argv[1], "http://", 7) == 0) 
    reader = oggplay_tcp_reader_new(argv[1], NULL, 0);
  else
    reader = oggplay_file_reader_new(argv[1]);

  shared_ptr<OggPlay> player(oggplay_open_with_reader(reader), oggplay_close);

  return 0;
}
