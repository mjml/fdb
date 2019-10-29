#include <iostream>

using namespace std;


/**
 * @brief This is called at the end of factorio startup.
 * @return
 */
int stub_init ()
{
  cout << "fdbstub_init()" << endl;
  cout.flush();
  return 84;
}

