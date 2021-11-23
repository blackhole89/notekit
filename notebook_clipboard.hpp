struct CClipboardDataRecord {
	guint8* markdown;
	gsize markdown_len;
	guint8* plaintext;
	gsize plaintext_len;};

static void
clipboard_clear_contents_cb (GtkClipboard *clipboard,
							 gpointer      data)
{
	CClipboardDataRecord *rec = (CClipboardDataRecord*)data;
	g_free(rec->markdown);
	g_free(rec->plaintext);
	delete rec;
}

static void
clipboard_get_contents_cb (GtkClipboard     *clipboard,
						   GtkSelectionData *selection_data,
						   guint             info,
						   gpointer          data)
{
	CClipboardDataRecord *rec = (CClipboardDataRecord*)data;
	if (info == GTK_TEXT_BUFFER_TARGET_INFO_RICH_TEXT)
	{
		gtk_selection_data_set(selection_data,gtk_selection_data_get_target (selection_data),8,rec->markdown,rec->markdown_len);
	} else {
		gtk_selection_data_set_text (selection_data, (gchar*)rec->plaintext, rec->plaintext_len);
	}
}

void cut_or_copy(GtkTextBuffer *buffer, GtkClipboard  *clipboard, bool delete_region_after, bool interactive, bool default_editable)
{
	GtkTextIter start;
	GtkTextIter end;
	
	printf("cut or copy\n");

	if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
	{
		/* Let's try the anchor thing */
		GtkTextMark * anchor = gtk_text_buffer_get_mark (buffer, "anchor");

		if (anchor == NULL)
			return;
		else
		{
			gtk_text_buffer_get_iter_at_mark (buffer, &end, anchor);
			gtk_text_iter_order (&start, &end);
		}
	}
	
	CClipboardDataRecord *rec = new CClipboardDataRecord;
	rec->markdown = gtk_text_buffer_serialize(buffer,buffer,gdk_atom_intern("text/notekit-markdown",true),&start,&end,&rec->markdown_len);
	rec->plaintext = gtk_text_buffer_serialize(buffer,buffer,gdk_atom_intern("text/plain",true),&start,&end,&rec->plaintext_len);
	//rec->plaintext = gtk_text_iter_get_visible_text (&start, &end);
	
	GtkTargetList *l = gtk_target_list_new(NULL,0);
	
	gtk_target_list_add_rich_text_targets (l,GTK_TEXT_BUFFER_TARGET_INFO_RICH_TEXT,true,buffer);
	gtk_target_list_add_text_targets (l,GTK_TEXT_BUFFER_TARGET_INFO_TEXT);
	
	GtkTargetEntry *t; gint n_targets;
	t = gtk_target_table_new_from_list (l, &n_targets);
	
	gtk_clipboard_set_with_data (clipboard,t,n_targets,clipboard_get_contents_cb,clipboard_clear_contents_cb, rec);

	gtk_target_table_free(t,n_targets);
	gtk_target_list_unref(l);

	if (delete_region_after)
	{
		if (interactive)
			gtk_text_buffer_delete_interactive (buffer, &start, &end,
												default_editable);
		else
			gtk_text_buffer_delete (buffer, &start, &end);
	}
}

void notebook_copy_clipboard (GtkTextView *text_view)
{
	GtkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view),
							  GDK_SELECTION_CLIPBOARD); 
	cut_or_copy(gtk_text_view_get_buffer(text_view),clipboard,false,true,true);
}

void notebook_cut_clipboard (GtkTextView *text_view)
{
	GtkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view),
							  GDK_SELECTION_CLIPBOARD); 
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
	
	gtk_text_buffer_begin_user_action (buffer);
	cut_or_copy (buffer, clipboard, true,true, gtk_text_view_get_editable(text_view) );
	gtk_text_buffer_end_user_action (buffer);
}

typedef struct
{
  GtkTextBuffer *buffer;
  GtkTextView *widget;
  guint interactive : 1;
  guint default_editable : 1;
  guint replace_selection : 1;
} ClipboardRequest;

static void
free_clipboard_request (ClipboardRequest *request_data)
{
	g_object_unref (request_data->buffer);
	g_slice_free (ClipboardRequest, request_data);
}

static void
pre_paste_prep (ClipboardRequest *request_data,
                GtkTextIter      *insert_point)
{
	GtkTextBuffer *buffer = request_data->buffer;

	gtk_text_buffer_get_iter_at_mark (buffer, insert_point, gtk_text_buffer_get_insert (buffer));

	if (request_data->replace_selection)
	{
		GtkTextIter start, end;
		  
		if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
		{
			if (request_data->interactive)
			gtk_text_buffer_delete_interactive (request_data->buffer,
												&start,
												&end,
												request_data->default_editable);
			else
				gtk_text_buffer_delete (request_data->buffer, &start, &end);

			*insert_point = start;
		}
	}
}

static void
clipboard_text_received (GtkClipboard *clipboard,
                         const gchar  *str,
                         gpointer      data)
{
	ClipboardRequest *request_data = (ClipboardRequest*)data;
	GtkTextBuffer *buffer = request_data->buffer;

	if (str)
	{
		GtkTextIter insert_point;
      
		if (request_data->interactive) 
			gtk_text_buffer_begin_user_action (buffer);

		pre_paste_prep (request_data, &insert_point);
      
		if (request_data->interactive) 
			gtk_text_buffer_insert_interactive (buffer, &insert_point,
											   str, -1, request_data->default_editable);
		else
			gtk_text_buffer_insert (buffer, &insert_point,
									str, -1);

		if (request_data->interactive) 
			gtk_text_buffer_end_user_action (buffer);

		//emit_paste_done (buffer, clipboard);
	}

	free_clipboard_request (request_data);
}

static void
clipboard_rich_text_received (GtkClipboard *clipboard,
							  GdkAtom       format,
							  const guint8 *text,
							  gsize         length,
							  gpointer      data)
{
	ClipboardRequest *request_data = (ClipboardRequest*)data;
	GtkTextIter insert_point;
	gboolean retval = TRUE;
	GError *error = NULL;

	if (text != NULL && length > 0)
	{
		if (request_data->interactive)
			gtk_text_buffer_begin_user_action (request_data->buffer);

		pre_paste_prep (request_data, &insert_point);

		if (!request_data->interactive ||
		     gtk_text_iter_can_insert (&insert_point,
									request_data->default_editable))
		{
		  retval = gtk_text_buffer_deserialize (request_data->buffer,
												request_data->buffer,
												format,
												&insert_point,
												text, length,
												&error);
		}

		if (!retval)
		{
			g_clear_error (&error);
			g_warning ("error pasting: %s\n", error->message);
		}

		if (request_data->interactive)
			gtk_text_buffer_end_user_action (request_data->buffer);

		//emit_paste_done (request_data->buffer, clipboard);

		if (retval) {
			free_clipboard_request (request_data);
			return;
		}
	}

	/* Request the text selection instead */
	gtk_clipboard_request_text (clipboard,
								clipboard_text_received,
								data);
}

static void
clipboard_image_received (GtkClipboard *clipboard,
                         GdkPixbuf *pixbuf,
                         gpointer data)
{
	ClipboardRequest *request_data = (ClipboardRequest*)data;
	GtkTextBuffer *buffer = request_data->buffer;
	
	CNotebook *nb = (CNotebook*)g_object_get_data(G_OBJECT(request_data->widget),"cppobj");

	if (pixbuf)
	{
		GtkTextIter insert_point;
      
		if (request_data->interactive) 
			gtk_text_buffer_begin_user_action (buffer);

		pre_paste_prep (request_data, &insert_point);
      
		if (request_data->interactive) 
			gtk_text_buffer_insert_interactive (buffer, &insert_point,
											   nb->DepositImage(pixbuf).c_str(), -1, request_data->default_editable);
		else
			gtk_text_buffer_insert (buffer, &insert_point,
									nb->DepositImage(pixbuf).c_str(), -1);

		if (request_data->interactive) 
			gtk_text_buffer_end_user_action (buffer);
	}

	free_clipboard_request (request_data);
}

void notebook_paste_clipboard (GtkTextView *text_view)
{
	GtkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view),
							  GDK_SELECTION_CLIPBOARD); 
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
	
	ClipboardRequest *data = g_slice_new (ClipboardRequest);
	GtkTextIter paste_point;
	GtkTextIter start, end;
	
	data->buffer = (GtkTextBuffer*)g_object_ref (buffer);
	data->widget = text_view;
	data->interactive = true;
	data->default_editable = gtk_text_view_get_editable(text_view);
	
	data->replace_selection = false;
  
	//get_paste_point (buffer, &paste_point, false);
	gtk_text_buffer_get_iter_at_mark (buffer, &paste_point,gtk_text_buffer_get_insert (buffer));
	
	if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end) &&
	  (gtk_text_iter_in_range (&paste_point, &start, &end) ||
	   gtk_text_iter_equal (&paste_point, &end)))
		data->replace_selection = true;
	
	if(gtk_clipboard_wait_is_image_available(clipboard)) {
	  /* Pasting image */
	  gtk_clipboard_request_image (clipboard,clipboard_image_received,data);
	} else if (gtk_clipboard_wait_is_rich_text_available (clipboard,buffer)) {
	  /* Request rich text */
	  gtk_clipboard_request_rich_text (clipboard,buffer,clipboard_rich_text_received,data);
	} else if(gtk_clipboard_wait_is_text_available (clipboard)) {
	  /* Request plain text instead */
	  gtk_clipboard_request_text (clipboard,clipboard_text_received,data);
	} 
}