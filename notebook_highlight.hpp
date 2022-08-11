std::string CNotebook::GetHighlightProxyDir()
{
	if(mkdir("/tmp/notekit.gsv",0777) && errno!=EEXIST) {
		printf("Failed to create a temporary directory for syntax highlighting data.\n");
		return "";
	}
	if(!access("/tmp/notekit.gsv/markdownlisting.lang", R_OK)) {
		// file already exists
		return "/tmp/notekit.gsv";
	}
	FILE *fl = fopen("/tmp/notekit.gsv/markdownlisting.lang","wb");
	if(!fl) {
		printf("Failed to create a language definition for syntax highlighting data.\n");
		return "";
	}
	
	/* start detecting languages */
	Glib::RefPtr<Gsv::LanguageManager> lm = Gsv::LanguageManager::get_default();
	std::vector<std::string> langs = lm->get_language_ids();
	std::vector<std::string> langs_supported;
	for(std::string &l : langs) {
		if(!lm->get_language(l)->get_hidden()) langs_supported.push_back(l);
	}
	
	/* generate code listing proxy lang definition */
	fprintf(fl,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n");
	fprintf(fl,"<language id=\"markdownlisting\" name=\"Markdown Code Listing\" version=\"2.0\" hidden=\"true\">\n <definitions>\n");
	
	/* one context for every language detected */
	for(std::string &l : langs_supported) {
		fprintf(fl,"  <context id=\"proxy-%s\" class=\"no-spell-check mono\">\n",l.c_str());
		fprintf(fl,"   <start>^(```\\s*)(%s(\\s.*)?)$</start>\n",l.c_str());
		fprintf(fl,"   <end>^(```)$</end>\n");
		fprintf(fl,"   <include>\n    <context id=\"proxy-%s-contents\" extend-parent=\"false\">\n",l.c_str());
		fprintf(fl,"    <start></start>\n");
		fprintf(fl,"    <include>\n     <context ref=\"%s:%s\" />\n    </include>\n",l.c_str(),l.c_str());
		fprintf(fl,"    </context>\n"
				   "    <context sub-pattern=\"1\" where=\"start\" style-ref=\"markdown:tag\" class=\"cbstart invis\" />\n"
				   "    <context sub-pattern=\"2\" where=\"start\" style-ref=\"markdown:known-lang\" class=\"cbtag\" />\n"
				   "    <context sub-pattern=\"1\" where=\"end\" style-ref=\"markdown:tag\" class=\"cbend invis\" />\n"
				   "   </include>\n"
		           "  </context>\n");
	}
	
	/* and one context to fallback when language not found */
	fprintf(fl,"  <context id=\"proxy-fallback\" class=\"no-spell-check mono\" style-ref=\"markdown:code\">\n"
			   "   <start>^(```\\s*)(.*)$</start>\n"
			   "   <end>^(```)$</end>\n"
			   "   <include>\n"
			   "     <context sub-pattern=\"1\" where=\"start\" style-ref=\"markdown:tag\" class=\"cbstart invis\" />\n"
			   "     <context sub-pattern=\"2\" where=\"start\" style-ref=\"markdown:unknown-lang\" class=\"cbtag\" />\n"
			   "     <context sub-pattern=\"1\" where=\"end\" style-ref=\"markdown:tag\" class=\"cbend invis\" />\n"
			   "   </include>\n"
	           "  </context>\n");
	
	/* finally, export every context as part of this "language" */
	fprintf(fl,"  <context id=\"markdownlisting\">\n   <include>\n");
	for(std::string &l : langs_supported) {
		fprintf(fl,"    <context ref=\"proxy-%s\"/>\n",l.c_str());
	}
	fprintf(fl,"    <context ref=\"proxy-fallback\"/>\n");
	fprintf(fl,"   </include>\n  </context>");
	
	fprintf(fl," </definitions>\n</language>\n");
	
	fclose(fl);
	
	return "/tmp/notekit.gsv";
}
