
// This file has been generated by genreflex with the --capabilities option
static  const char* clnames[] = {
//--Begin classes
  "LCGReflex/merge::ScanInfo",
  "LCGReflex/std::vector<merge::ScanInfo>",
  "LCGReflex/merge::MINOS",
  "LCGReflex/std::vector<merge::MINOS>",
  "LCGReflex/merge::Paddles",
  "LCGReflex/std::vector<merge::Paddles>",
  "LCGReflex/edm::Wrapper<std::vector<merge::Paddles> >",
  "LCGReflex/edm::Wrapper<std::vector<merge::MINOS> >",
  "LCGReflex/edm::Wrapper<std::vector<merge::ScanInfo> >",
//--End   classes
//--Final End
};

extern "C" void SEAL_CAPABILITIES (const char**& names, int& n )
{ 
  names = clnames;
  n = sizeof(clnames)/sizeof(char*);
}

