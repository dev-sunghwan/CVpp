import fitz  # PyMuPDF
import sys

pdf_path = "C:/Users/SungHwan/Desktop/CV++/SUNAPI_video_audio_2.6.7.pdf"

try:
    doc = fitz.open(pdf_path)
    text_lines = []
    
    # We are looking for "RTSP", "Range", "Scale", "Rate-Control", "Custom Header"
    keywords = ["RTSP", "Range", "Scale", "Rate-Control", "header"]
    
    for page_num in range(len(doc)):
        page = doc[page_num]
        text = page.get_text()
        blocks = text.split('\n')
        
        for i, line in enumerate(blocks):
            if any(k.lower() in line.lower() for k in keywords):
                # Print the line and some context
                start = max(0, i - 2)
                end = min(len(blocks), i + 3)
                context = " ".join(blocks[start:end])
                text_lines.append(f"Page {page_num + 1}: {context}")
                
    with open("pdf_extract.txt", "w", encoding="utf-8") as f:
        for line in text_lines:
            f.write(line + "\n")
            
    print(f"Extraction complete. Found {len(text_lines)} matches.")
except Exception as e:
    print(f"Error: {e}")
