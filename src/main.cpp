#include "webserv.h"

volatile sig_atomic_t sig = 0;

int main(const int argc, char *argv[]) {
  (void)argc;
  if (argc != 2) {
    std::cerr << "Usage: webserv <config_file>" << std::endl;
    return 1;
  }
  Result<FileDescriptor> fd = FileDescriptor::open_file(argv[1]);
  if (!fd.error().empty()) {
    std::cerr << "file open failed: " << fd.error() << std::endl;
    return 1;
  }
  Result<WebserverConfig> result_config =
      WebserverConfig::parse(fd.value_mut());
  if (!result_config.error().empty()) {
    std::cerr << "config parsing failed: " << result_config.error()
              << std::endl;
    return 1;
  } else {
    const WebserverConfig &config = result_config.value();
    std::cout << "main(): " << std::endl;
    ServerConfig const &sconf = config.Get_ServerConfig_map().at(80);
  
  












    // Test the new findRoute method
  //   std::cout << "\n=== Testing Route Matching ===" << std::endl;
  //   RouteRule const *rule = sconf.findRoute(Http::GET, "old_stuff");
  //   if (rule) {
  //     std::cout << "Found route for GET old_stuff: " << rule->path.toString() << std::endl;
  //   } else {
  //     std::cout << "No route found for GET old_stuff" << std::endl;
  //   }
    
  //   // Try with a path that should match wildcard
  //   rule = sconf.findRoute(Http::GET, "/some/path/image.jpg");
  //   if (rule) {
  //     std::cout << "Found route for GET /some/path/image.jpg: " << rule->path.toString() << std::endl;
  //   } else {
  //     std::cout << "No route found for GET /some/path/image.jpg" << std::endl;
  //   }
    
  //   // Try POST wildcard
  //   rule = sconf.findRoute(Http::POST, "/any/path");
  //   if (rule) {
  //     std::cout << "Found route for POST /any/path: " << rule->path.toString() << std::endl;
  //   } else {
  //     std::cout << "No route found for POST /any/path" << std::endl;
  //   }
  // }
  
  // ===== Pattern Matching Tests =====
  // std::cout << "\n========== PathPattern Matching Tests ==========" << std::endl;
  
  // // Test 1: "*" matches everything
  // std::cout << "\nTest 1: '*' matches everything" << std::endl;
  // PathPattern wildcard("*");
  // std::cout << "  '*' matches 'anything': " << (wildcard.matches("anything") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*' matches '/path/to/file': " << (wildcard.matches("/path/to/file") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*' matches 'a': " << (wildcard.matches("a") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*' matches '': " << (wildcard.matches("") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 2: "*.jpg" matches any path with .jpg
  // std::cout << "\nTest 2: '*.jpg' matches files ending with .jpg" << std::endl;
  // PathPattern jpgPattern("*.jpg");
  // std::cout << "  '*.jpg' matches 'picture1.jpg': " << (jpgPattern.matches("picture1.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*.jpg' matches 'photo.jpg': " << (jpgPattern.matches("photo.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*.jpg' matches '/some/path/pic2.jpg': " << (jpgPattern.matches("/some/path/pic2.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*.jpg' matches '/images/photo.jpg': " << (jpgPattern.matches("/images/photo.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*.jpg' matches 'image.png': " << (!jpgPattern.matches("image.png") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*.jpg' matches 'file.txt': " << (!jpgPattern.matches("file.txt") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 3: "*.png" matches .png files
  // std::cout << "\nTest 3: '*.png' matches files ending with .png" << std::endl;
  // PathPattern pngPattern("*.png");
  // std::cout << "  '*.png' matches 'image.png': " << (pngPattern.matches("image.png") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*.png' matches '/path/to/image.png': " << (pngPattern.matches("/path/to/image.png") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*.png' matches 'photo.jpg': " << (!pngPattern.matches("photo.jpg") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 4: Patterns with prefix
  // std::cout << "\nTest 4: 'img*.jpg' matches files starting with img and ending with .jpg" << std::endl;
  // PathPattern imgPattern("img*.jpg");
  // std::cout << "  'img*.jpg' matches 'img1.jpg': " << (imgPattern.matches("img1.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'img*.jpg' matches 'img_photo.jpg': " << (imgPattern.matches("img_photo.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'img*.jpg' matches '/path/img_photo.jpg': " << (imgPattern.matches("/path/img_photo.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'img*.jpg' matches 'photo.jpg': " << (!imgPattern.matches("photo.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'img*.jpg' matches 'img.png': " << (!imgPattern.matches("img.png") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 5: Multi-segment patterns
  // std::cout << "\nTest 5: '/images/*.jpg' matches .jpg files in /images/" << std::endl;
  // PathPattern imagesJpg("/images/*.jpg");
  // std::cout << "  '/images/*.jpg' matches '/images/photo.jpg': " << (imagesJpg.matches("/images/photo.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/images/*.jpg' matches '/images/pic1.jpg': " << (imagesJpg.matches("/images/pic1.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/images/*.jpg' matches '/images/photo.png': " << (!imagesJpg.matches("/images/photo.png") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/images/*.jpg' matches '/other/photo.jpg': " << (!imagesJpg.matches("/other/photo.jpg") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 6: Path with wildcard in middle
  // std::cout << "\nTest 6: '/path/*/file' matches file in any subdirectory of /path/" << std::endl;
  // PathPattern pathWildFile("/path/*/file");
  // std::cout << "  '/path/*/file' matches '/path/any/file': " << (pathWildFile.matches("/path/any/file") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/path/*/file' matches '/path/subdir/file': " << (pathWildFile.matches("/path/subdir/file") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/path/*/file' matches '/path/other/file': " << (pathWildFile.matches("/path/other/file") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/path/*/file' matches '/path/any/other': " << (!pathWildFile.matches("/path/any/other") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/path/*/file' matches '/other/any/file': " << (!pathWildFile.matches("/other/any/file") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 7: Exact match (no wildcards)
  // std::cout << "\nTest 7: Exact match without wildcards" << std::endl;
  // PathPattern exactPath("/exact/path/file.txt");
  // std::cout << "  '/exact/path/file.txt' matches '/exact/path/file.txt': " << (exactPath.matches("/exact/path/file.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/exact/path/file.txt' matches '/exact/path/other.txt': " << (!exactPath.matches("/exact/path/other.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '/exact/path/file.txt' matches '/other/path/file.txt': " << (!exactPath.matches("/other/path/file.txt") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 8: Complex patterns
  // std::cout << "\nTest 8: Complex wildcard patterns" << std::endl;
  // PathPattern complexPattern("test*.txt");
  // std::cout << "  'test*.txt' matches 'test.txt': " << (complexPattern.matches("test.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'test*.txt' matches 'test123.txt': " << (complexPattern.matches("test123.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'test*.txt' matches 'test_file.txt': " << (complexPattern.matches("test_file.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'test*.txt' matches '/dir/test_file.txt': " << (complexPattern.matches("/dir/test_file.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'test*.txt' matches 'other.txt': " << (!complexPattern.matches("other.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  'test*.txt' matches 'test.jpg': " << (!complexPattern.matches("test.jpg") ? "PASS" : "FAIL") << std::endl;
  
  // // Test 9: Multiple wildcards in a segment
  // std::cout << "\nTest 9: Multiple wildcards" << std::endl;
  // PathPattern multiWild("*img*");
  // std::cout << "  '*img*' matches 'img.jpg': " << (multiWild.matches("img.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*img*' matches 'myimg.png': " << (multiWild.matches("myimg.png") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*img*' matches 'test_img_file.txt': " << (multiWild.matches("test_img_file.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*img*' matches '/path/test_img_file.txt': " << (multiWild.matches("/path/test_img_file.txt") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*img*' matches 'photo.jpg': " << (!multiWild.matches("photo.jpg") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '*img*' matches 'image.jpg': " << (!multiWild.matches("image.jpg") ? "PASS (no 'img' substring)" : "FAIL") << std::endl;
  
  // // Test 10: Edge cases
  // std::cout << "\nTest 10: Edge cases" << std::endl;
  // PathPattern starOnly("*");
  // PathPattern starStarPattern("**");
  // std::cout << "  '*' matches '*': " << (starOnly.matches("*") ? "PASS" : "FAIL") << std::endl;
  // std::cout << "  '**' matches 'anything': " << (starStarPattern.matches("anything") ? "PASS" : "FAIL") << std::endl;
  
  // std::cout << "\n========== End of Pattern Matching Tests ==========" << std::endl;
  
  // return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }
