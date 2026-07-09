#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// 이미지는 Texture 클래스에서 직접 로드한다(KTX2 포함). tinygltf가 외부 이미지
// 파일을 읽거나 stb로 디코드하지 않고 image.uri만 보존하도록 함.
// (bistro의 .ktx2는 stb가 디코드 불가 → 이 매크로가 없으면 파싱 자체가 실패)
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tinygltf/tiny_gltf.h>