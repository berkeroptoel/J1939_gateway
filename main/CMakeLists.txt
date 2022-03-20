idf_component_register(SRCS "main.c" "ota_op.c" "wifi_op.c" "file_op.c"
							"twai.c" "mqtt_pub.c" "mqtt_sub.c"
                    INCLUDE_DIRS "."
                    # Embed the server root certificate into the final binary
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
                    
                    
# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
spiffs_create_partition_image(storage ../Records FLASH_IN_PROJECT)

