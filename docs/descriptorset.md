
frequency + shader reflect

mental model of resource binding

UPDATE_FREQ_NONE // resource update only once
UPDATE_FREQ_PER_FRAME
UPDATE_FREQ_PER_BATCH
UPDATE_FREQ_PER_DRAW

https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/

https://ruby0x1.github.io/machinery_blog_archive/post/efficient-binding-of-shader-resources/

https://themaister.net/blog/2019/04/20/a-tour-of-granites-vulkan-backend-part-3/

resource - pipeline

pipeline access resource


pipeline->bindresource
pipeline->updateresources(freq)


drawback

resource shared by multiple pass

each pipeline owns a descriptorset