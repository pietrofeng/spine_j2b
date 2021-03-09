# spine_j2b

1.把spine动画json文件数据转换为二进制数据
2.使用二进制可以大幅提高加载速度
3.基于cocos2d-x3.17.2版本spine代码开发

API
int convert_json_to_binary(const char *json, size_t len, unsigned char *outBuff,const char *atlas = 0);
    
建议调用时传入atlas数据，调用时传入atlas数据可以提前过滤掉json和atlas不匹配的attachment，避免一些闪退的问题。



